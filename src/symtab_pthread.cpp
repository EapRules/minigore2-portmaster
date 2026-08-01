/*
 * pthread_attr_t, because bionic's is 24 bytes and glibc's is 36.
 *
 * bionic (ILP32) declares the attribute object inline:
 *
 *     typedef struct {
 *       uint32_t flags;  void *stack_base;   size_t stack_size;
 *       size_t guard_size;  int32_t sched_policy;  int32_t sched_priority;
 *     } pthread_attr_t;                                  // 24 bytes
 *
 * glibc declares it as `union { char __size[36]; long __align; }`. The
 * generated bionic table binds pthread_attr_init straight to the host's, and
 * the host's zeroes all 36 bytes - into the 24 the game reserved.
 *
 * native_app_glue puts that object on the stack:
 *
 *     android_app_create():
 *         pthread_attr_t attr;                  // 24 bytes of frame
 *         pthread_attr_init(&attr);             // host writes 36
 *         pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
 *         pthread_create(&app->thread, &attr, android_app_entry, app);
 *
 * so the twelve extra bytes land on whatever the compiler put after it -
 * here, the stack canary - and the process dies in __stack_chk_fail during
 * ANativeActivity_onCreate, before the game thread has done anything. Exactly
 * the same failure shape as clock_gettime in symtab_time.cpp, and just as
 * silent: nothing in the log mentions pthread.
 *
 * So the attribute object is kept in the game's layout on the game's stack,
 * and translated into a real host pthread_attr_t only at pthread_create.
 *
 * These entries come before the generated libc table in so_dynamic_libraries
 * so they win the lookup.
 */
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "so_util.h"
#include "thunk_gen.h"

extern "C" {

struct bionic_pthread_attr {
    uint32_t flags;
    void    *stack_base;
    uint32_t stack_size;
    uint32_t guard_size;
    int32_t  sched_policy;
    int32_t  sched_priority;
};

/* bionic/pthread_internal.h */
enum {
    BIONIC_PTHREAD_ATTR_FLAG_DETACHED = 0x00000001,
    BIONIC_PTHREAD_ATTR_FLAG_INHERIT  = 0x00000004,
};

/* bionic's own defaults for a 32-bit process. */
static const uint32_t kBionicDefaultStack = 1 * 1024 * 1024;
static const uint32_t kBionicDefaultGuard = 4096;

int bionic_pthread_attr_init(struct bionic_pthread_attr *attr)
{
    if (!attr)
        return EINVAL;

    memset(attr, 0, sizeof(*attr));
    attr->stack_size    = kBionicDefaultStack;
    attr->guard_size    = kBionicDefaultGuard;
    attr->sched_policy  = SCHED_OTHER;
    attr->sched_priority = 0;
    return 0;
}

int bionic_pthread_attr_destroy(struct bionic_pthread_attr *attr)
{
    if (!attr)
        return EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int bionic_pthread_attr_setdetachstate(struct bionic_pthread_attr *attr, int state)
{
    if (!attr)
        return EINVAL;

    /* PTHREAD_CREATE_JOINABLE == 0 and PTHREAD_CREATE_DETACHED == 1 on both
     * sides, so the constant crosses unchanged; only the storage differs. */
    if (state == PTHREAD_CREATE_DETACHED)
        attr->flags |= BIONIC_PTHREAD_ATTR_FLAG_DETACHED;
    else if (state == PTHREAD_CREATE_JOINABLE)
        attr->flags &= ~(uint32_t)BIONIC_PTHREAD_ATTR_FLAG_DETACHED;
    else
        return EINVAL;

    return 0;
}

int bionic_pthread_attr_getdetachstate(const struct bionic_pthread_attr *attr, int *state)
{
    if (!attr || !state)
        return EINVAL;
    *state = (attr->flags & BIONIC_PTHREAD_ATTR_FLAG_DETACHED)
                 ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
    return 0;
}

int bionic_pthread_attr_setstacksize(struct bionic_pthread_attr *attr, uint32_t size)
{
    if (!attr || size < 16384)
        return EINVAL;
    attr->stack_size = size;
    return 0;
}

int bionic_pthread_attr_getstacksize(const struct bionic_pthread_attr *attr, uint32_t *size)
{
    if (!attr || !size)
        return EINVAL;
    *size = attr->stack_size;
    return 0;
}

int bionic_pthread_attr_setguardsize(struct bionic_pthread_attr *attr, uint32_t size)
{
    if (!attr)
        return EINVAL;
    attr->guard_size = size;
    return 0;
}

/*
 * The counterpart: read the game's attribute object and build a host one.
 *
 * The generated table's pthread_create thunk ignores the attribute argument
 * entirely - safe, since it cannot parse it, but it means a thread the game
 * detached stays joinable and is never reaped. native_app_glue detaches its
 * thread and never joins it, so honouring the flag is what keeps a long
 * session from accumulating dead thread descriptors.
 *
 * The stack size is deliberately *not* honoured downwards. bionic's 1 MB
 * default is sized for a thread that only ever runs the game; here the same
 * thread also runs host GL, SDL and libc code that Android's linker never had
 * to fit in that budget, so the host default is used unless the game asked
 * for more.
 */
int bionic_pthread_create(pthread_t *thread, const struct bionic_pthread_attr *attr,
                          void *(*entry)(void *), void *arg)
{
    if (!attr)
        return pthread_create(thread, NULL, entry, arg);

    pthread_attr_t host;
    if (pthread_attr_init(&host) != 0)
        return pthread_create(thread, NULL, entry, arg);

    if (attr->flags & BIONIC_PTHREAD_ATTR_FLAG_DETACHED)
        pthread_attr_setdetachstate(&host, PTHREAD_CREATE_DETACHED);

    size_t host_default = 0;
    pthread_attr_getstacksize(&host, &host_default);
    if (attr->stack_size > host_default)
        pthread_attr_setstacksize(&host, attr->stack_size);

    int rc = pthread_create(thread, &host, entry, arg);
    pthread_attr_destroy(&host);
    return rc;
}

} /* extern "C" */

DynLibFunction symtable_pthread[] = {
    THUNK_SPECIFIC("pthread_attr_init",           bionic_pthread_attr_init),
    THUNK_SPECIFIC("pthread_attr_destroy",        bionic_pthread_attr_destroy),
    THUNK_SPECIFIC("pthread_attr_setdetachstate", bionic_pthread_attr_setdetachstate),
    THUNK_SPECIFIC("pthread_attr_getdetachstate", bionic_pthread_attr_getdetachstate),
    THUNK_SPECIFIC("pthread_attr_setstacksize",   bionic_pthread_attr_setstacksize),
    THUNK_SPECIFIC("pthread_attr_getstacksize",   bionic_pthread_attr_getstacksize),
    THUNK_SPECIFIC("pthread_attr_setguardsize",   bionic_pthread_attr_setguardsize),
    THUNK_SPECIFIC("pthread_create",              bionic_pthread_create),
    { NULL, 0 },
};
