/*
 * Shader compilation probe.
 *
 * M4's criterion is "the EGL context is current and the game's shaders
 * compile". The loader cannot answer that by inspection: the engine compiles
 * its shaders from its own thread, checks the status itself, and says nothing
 * on stderr when one fails - it just draws nothing afterwards, which looks
 * exactly like a working port with a black frame.
 *
 * So glCompileShader and glLinkProgram are intercepted. Each call is forwarded
 * to the real driver entry point and the status is read back; a failure is
 * printed with the driver's info log, and the counts are what the loader waits
 * on before it is willing to claim GL is up.
 *
 * This is observation, not emulation: the driver does the work and its verdict
 * is reported verbatim. A shader that does not compile makes the milestone
 * fail, which is the point.
 *
 * The table below is listed *before* symtable_gles2 in so_dynamic_libraries,
 * so the game binds these instead of the raw entry points. The glad_gl*
 * pointers still hold the driver's own functions - the same split the vendored
 * glShaderSource_dump uses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>

#include "khronos/glad.h"

#include "so_util.h"
#include "thunk_gen.h"
#include "trace.h"
#include "viewport_scale.h"

/* Written by the game's thread, read by the loader's. */
static std::atomic<int> g_shaders_ok(0);
static std::atomic<int> g_shaders_failed(0);
static std::atomic<int> g_programs_ok(0);
static std::atomic<int> g_programs_failed(0);
static std::atomic<long> g_draws(0);

static void dump_log(const char *what, unsigned int obj, bool is_shader)
{
    GLint len = 0;
    if (is_shader)
        glad_glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len);
    else
        glad_glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);

    if (len <= 1) {
        fprintf(stderr, "GL: %s failed (driver gave no log)\n", what);
        fflush(stderr);
        return;
    }

    char *log = (char *)malloc((size_t)len + 1);
    if (!log)
        return;
    log[0] = '\0';
    if (is_shader)
        glad_glGetShaderInfoLog(obj, len, NULL, log);
    else
        glad_glGetProgramInfoLog(obj, len, NULL, log);
    log[len] = '\0';

    fprintf(stderr, "GL: %s failed:\n%s\n", what, log);
    fflush(stderr);
    free(log);
}

extern "C" void probe_glCompileShader(GLuint shader)
{
    glad_glCompileShader(shader);

    GLint ok = 0;
    glad_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok) {
        g_shaders_ok++;
        return;
    }

    g_shaders_failed++;
    dump_log("shader compile", shader, true);
}

extern "C" void probe_glLinkProgram(GLuint program)
{
    glad_glLinkProgram(program);

    GLint ok = 0;
    glad_glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok) {
        g_programs_ok++;
        return;
    }

    g_programs_failed++;
    dump_log("program link", program, false);
}

/*
 * Draw calls.
 *
 * GLES2 has exactly two of them, and the engine's imports confirm it uses
 * both. Counting here rather than in the frame loop is the difference the
 * milestone cares about: a game that clears the screen to a colour and
 * presents it swaps buffers just as happily as one that renders, so frames are
 * not evidence of content. A draw call is.
 *
 * Neither wrapper inspects or rewrites its arguments; they are forwarded
 * verbatim to the driver, so a miscount is the only thing that can go wrong
 * here, never a misdraw.
 */
extern "C" void probe_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    g_draws++;
    glad_glDrawArrays(mode, first, count);
}

extern "C" void probe_glDrawElements(GLenum mode, GLsizei count, GLenum type,
                                     const void *indices)
{
    g_draws++;
    glad_glDrawElements(mode, count, type, indices);
}

/*
 * Aspect-correct scaling to the device's real panel.
 *
 * Only used when the engine cannot be told the panel's real size directly -
 * see MINIGORE_RENDER in main.cpp. In that case the engine renders at one
 * fixed logical size and issues a single full-screen glViewport at that size,
 * with glScissor calls in the same space; on a larger panel that viewport
 * lands in the bottom-left corner and the rest of the screen stays black. We
 * remap that one full-screen viewport onto the real drawable and apply the
 * matching affine transform to scissor rectangles so clipped UI stays aligned.
 * Every other viewport - a render target of a different size - is left
 * untouched, since that is exactly the call blind remapping would break.
 *
 * MINIGORE_SCALE picks the policy:
 *   fit      (default) keep the logical aspect, letterbox, no distortion
 *   stretch  fill the panel, minor distortion
 *   integer  largest whole-number fit, letterboxed
 *
 * On a panel that already is the logical size (the R36S) this is identity and
 * every probe below forwards its arguments unchanged.
 */
enum { SCALE_FIT = 0, SCALE_STRETCH = 1, SCALE_INTEGER = 2 };

static int     g_scale_active = 0;
static int     g_log_w = 640, g_log_h = 480;   /* what the engine thinks it has */
static int     g_phys_w = 0, g_phys_h = 0;     /* the real drawable             */
static GLint   g_dst_x = 0, g_dst_y = 0;       /* full-screen rect on the panel */
static GLsizei g_dst_w = 0, g_dst_h = 0;
static float   g_scale_x = 1.0f, g_scale_y = 1.0f;

extern "C" void viewport_scale_init(int phys_w, int phys_h,
                                    int log_w, int log_h)
{
    g_log_w = log_w > 0 ? log_w : 640;
    g_log_h = log_h > 0 ? log_h : 480;
    g_phys_w = phys_w;
    g_phys_h = phys_h;

    /* No panel info, or the panel already matches the logical size: pass every
     * call through untouched. This is the R36S path - identity, zero cost, and
     * also the whole of the native path, where the two are equal by
     * construction. */
    if (phys_w <= 0 || phys_h <= 0 ||
        (phys_w == g_log_w && phys_h == g_log_h)) {
        g_scale_active = 0;
        trace("viewport scale: identity (panel %dx%d, logical %dx%d)",
              phys_w, phys_h, g_log_w, g_log_h);
        return;
    }

    const char *m = getenv("MINIGORE_SCALE");
    int mode = SCALE_FIT;
    if (m && !strcmp(m, "stretch"))      mode = SCALE_STRETCH;
    else if (m && !strcmp(m, "integer")) mode = SCALE_INTEGER;

    if (mode == SCALE_STRETCH) {
        g_dst_x = 0; g_dst_y = 0;
        g_dst_w = phys_w; g_dst_h = phys_h;
    } else {
        float sx = (float)phys_w / (float)g_log_w;
        float sy = (float)phys_h / (float)g_log_h;
        float s  = sx < sy ? sx : sy;          /* largest that fits          */
        if (mode == SCALE_INTEGER) {
            s = (float)(int)s;                 /* floor to a whole multiple  */
            if (s < 1.0f) s = 1.0f;
        }
        g_dst_w = (GLsizei)(g_log_w * s + 0.5f);
        g_dst_h = (GLsizei)(g_log_h * s + 0.5f);
        g_dst_x = (phys_w - g_dst_w) / 2;
        g_dst_y = (phys_h - g_dst_h) / 2;
    }

    g_scale_x = (float)g_dst_w / (float)g_log_w;
    g_scale_y = (float)g_dst_h / (float)g_log_h;
    g_scale_active = 1;
    trace("viewport scale: %s panel=%dx%d logical=%dx%d -> dst=%d,%d %dx%d",
          mode == SCALE_STRETCH ? "stretch" :
          mode == SCALE_INTEGER ? "integer" : "fit",
          phys_w, phys_h, g_log_w, g_log_h,
          g_dst_x, g_dst_y, g_dst_w, g_dst_h);
}

/*
 * The same affine the scissor path applies, for the port's own overlay.
 *
 * The software cursor lives in the engine's logical space - platform.cpp
 * clamps it there because that is what the touch events must carry - but it is
 * painted straight into the physical framebuffer. Without this the arrow could
 * only reach the logical rectangle in the corner of a larger panel.
 *
 * Y is top-left-origin here, unlike glScissor's bottom-left, so the offset is
 * the letterbox bar above the content rather than the one below it.
 */
extern "C" void viewport_scale_map(float *x, float *y)
{
    if (!g_scale_active)
        return;
    if (x)
        *x = (float)g_dst_x + *x * g_scale_x;
    if (y)
        *y = (float)(g_phys_h - g_dst_y - g_dst_h) + *y * g_scale_y;
}

extern "C" int viewport_scale_factor(void)
{
    if (!g_scale_active)
        return 1;
    float s = g_scale_x < g_scale_y ? g_scale_x : g_scale_y;
    int factor = (int)(s + 0.5f);
    return factor < 1 ? 1 : factor;
}

/* The engine's one full-screen viewport, the only one we remap. */
static inline int is_fullscreen_viewport(GLint x, GLint y,
                                         GLsizei w, GLsizei h)
{
    return g_scale_active && x == 0 && y == 0 &&
           w == g_log_w && h == g_log_h;
}

/*
 * Geometry diagnostics for real devices, and the scaling seam.
 *
 * A black or cropped frame can be a correct draw into the wrong rectangle, and
 * these two lines in the log are what tell the difference. They are also the
 * evidence for whether the engine adapts to a non-4:3 surface at all: on the
 * native path nothing rewrites these arguments, so what is logged is the
 * engine's own opinion of the screen it was given. Logging only changes, and
 * only the first few, keeps LOADER_TRACE readable.
 */
extern "C" void probe_glViewport(GLint x, GLint y, GLsizei width,
                                 GLsizei height)
{
    if (is_fullscreen_viewport(x, y, width, height)) {
        x = g_dst_x; y = g_dst_y; width = g_dst_w; height = g_dst_h;
    }

    static GLint last_x = -1, last_y = -1;
    static GLsizei last_w = -1, last_h = -1;
    static int lines = 0;

    if (lines < 24 &&
        (x != last_x || y != last_y || width != last_w || height != last_h)) {
        trace("GL viewport: x=%d y=%d width=%d height=%d", x, y, width, height);
        last_x = x; last_y = y; last_w = width; last_h = height;
        lines++;
    }
    if (glad_glViewport)
        glad_glViewport(x, y, width, height);
}

extern "C" void probe_glScissor(GLint x, GLint y, GLsizei width,
                                GLsizei height)
{
    /* Every scissor is in the logical panel's space (the engine uses one
     * viewport), so the same affine transform keeps clipped UI aligned with
     * the scaled content. Identity when scaling is off. */
    if (g_scale_active) {
        x = g_dst_x + (GLint)(x * g_scale_x + 0.5f);
        y = g_dst_y + (GLint)(y * g_scale_y + 0.5f);
        width  = (GLsizei)(width  * g_scale_x + 0.5f);
        height = (GLsizei)(height * g_scale_y + 0.5f);
    }

    static GLint last_x = -1, last_y = -1;
    static GLsizei last_w = -1, last_h = -1;
    static int lines = 0;

    if (lines < 24 &&
        (x != last_x || y != last_y || width != last_w || height != last_h)) {
        trace("GL scissor: x=%d y=%d width=%d height=%d", x, y, width, height);
        last_x = x; last_y = y; last_w = width; last_h = height;
        lines++;
    }
    if (glad_glScissor)
        glad_glScissor(x, y, width, height);
}

extern "C" long android_gl_draw_calls(void) { return g_draws.load(); }

extern "C" int android_gl_shaders_compiled(void) { return g_shaders_ok.load(); }
extern "C" int android_gl_shaders_failed(void)   { return g_shaders_failed.load(); }
extern "C" int android_gl_programs_linked(void)  { return g_programs_ok.load(); }
extern "C" int android_gl_programs_failed(void)  { return g_programs_failed.load(); }

/*
 * Neither function takes or returns a float, so select_either() hands the game
 * the pointer directly with no ABI bridge - same as the entries these shadow
 * in symtable_gles2.
 */
DynLibFunction symtable_glprobe[] = {
    THUNK_SPECIFIC("glCompileShader", probe_glCompileShader),
    THUNK_SPECIFIC("glLinkProgram",   probe_glLinkProgram),
    THUNK_SPECIFIC("glDrawArrays",    probe_glDrawArrays),
    THUNK_SPECIFIC("glDrawElements",  probe_glDrawElements),
    THUNK_SPECIFIC("glViewport",      probe_glViewport),
    THUNK_SPECIFIC("glScissor",       probe_glScissor),
    { NULL, 0 },
};
