/*
 * libm.so, plus the two libc entry points the generated bionic table cannot
 * express.
 *
 * gmloader-next deliberately leaves the math functions out of its libc table
 * (thunks/libc/symtab_exclude) because it loads a real bionic libm.so next to
 * the game. This port does not: the host's libm is the same IEEE-754 double
 * math, and routing to it avoids shipping an AOSP binary blob with the port.
 *
 * The catch is the ABI. The game is armeabi-v7a, which passes and returns
 * floats in core registers; this loader is hardfp and uses VFP registers. Every
 * function below therefore crosses an ABI boundary, and THUNK_SPECIFIC inserts
 * the bridge - without it cos() would read its argument out of the wrong
 * register and quietly return garbage, which is far worse than not resolving.
 *
 * The declarations live in a namespace so the C++ float/long-double overloads
 * of <cmath> cannot make &cos ambiguous; extern "C" keeps the symbol names, so
 * these still bind to plain libm at link time. Same trick libc_table.cpp uses.
 */
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "so_util.h"
#include "thunk_gen.h"

namespace HOST_MATH {
extern "C" {
double acos(double);
double atan(double);
double atan2(double, double);
double ceil(double);
double cos(double);
double exp(double);
double floor(double);
double fmod(double, double);
double log(double);
double pow(double, double);
double rint(double);
double sin(double);
double sqrt(double);
double tan(double);

float asinf(float);
float atan2f(float, float);
float cosf(float);
float floorf(float);
float fmodf(float, float);
float logf(float);
float powf(float, float);
float rintf(float);
float sinf(float);
float sqrtf(float);
float tanf(float);
}
}

/*
 * open() and ioctl() are variadic in bionic, and the table generator skips
 * variadic functions because it cannot forward an unknown argument list
 * through a thunk. They are unpacked by hand here instead.
 *
 * Neither takes a float, so no ABI bridge is involved: the variable part of an
 * AAPCS argument list is passed identically under soft-float and hard-float.
 */
extern "C" int minigore_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    return open(path, flags, mode);
}

extern "C" int minigore_ioctl(int fd, int request, ...)
{
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    return ioctl(fd, (unsigned long)request, arg);
}

DynLibFunction symtable_libm[] = {
    THUNK_SPECIFIC("acos",   HOST_MATH::acos),
    THUNK_SPECIFIC("atan",   HOST_MATH::atan),
    THUNK_SPECIFIC("atan2",  HOST_MATH::atan2),
    THUNK_SPECIFIC("ceil",   HOST_MATH::ceil),
    THUNK_SPECIFIC("cos",    HOST_MATH::cos),
    THUNK_SPECIFIC("exp",    HOST_MATH::exp),
    THUNK_SPECIFIC("floor",  HOST_MATH::floor),
    THUNK_SPECIFIC("fmod",   HOST_MATH::fmod),
    THUNK_SPECIFIC("log",    HOST_MATH::log),
    THUNK_SPECIFIC("pow",    HOST_MATH::pow),
    THUNK_SPECIFIC("rint",   HOST_MATH::rint),
    THUNK_SPECIFIC("sin",    HOST_MATH::sin),
    THUNK_SPECIFIC("sqrt",   HOST_MATH::sqrt),
    THUNK_SPECIFIC("tan",    HOST_MATH::tan),

    THUNK_SPECIFIC("asinf",  HOST_MATH::asinf),
    THUNK_SPECIFIC("atan2f", HOST_MATH::atan2f),
    THUNK_SPECIFIC("cosf",   HOST_MATH::cosf),
    THUNK_SPECIFIC("floorf", HOST_MATH::floorf),
    THUNK_SPECIFIC("fmodf",  HOST_MATH::fmodf),
    THUNK_SPECIFIC("logf",   HOST_MATH::logf),
    THUNK_SPECIFIC("powf",   HOST_MATH::powf),
    THUNK_SPECIFIC("rintf",  HOST_MATH::rintf),
    THUNK_SPECIFIC("sinf",   HOST_MATH::sinf),
    THUNK_SPECIFIC("sqrtf",  HOST_MATH::sqrtf),
    THUNK_SPECIFIC("tanf",   HOST_MATH::tanf),

    NO_THUNK("open",  (uintptr_t)&minigore_open),
    NO_THUNK("ioctl", (uintptr_t)&minigore_ioctl),

    { NULL, 0 },
};
