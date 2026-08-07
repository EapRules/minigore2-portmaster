#pragma once

/*
 * The one way the game is allowed to end itself.
 *
 * The engine leaves by calling ANativeActivity_finish(): on Android the
 * framework tears the activity down and the process follows. Here there is no
 * framework, so the request has to be recorded and acted on by the loader's
 * own loop - never where it is raised. The engine calls finish() from inside
 * its own call stack, on its own thread, and unwinding the process from under
 * it is a crash on the way out rather than an exit.
 *
 * This replaces a plain bool written on the game's thread and read on the
 * loader's. That was a data race, and on a weakly ordered ARM the reader can
 * sit on a stale value indefinitely - a quit from the game's menu that leaves
 * the port running, which from the couch is a freeze.
 */

/* Ask the frame loop to stop. Safe from any thread, and idempotent: the
 * engine can call finish() more than once and only the first is announced. */
void android_app_request_exit(const char *reason);

/* True once android_app_request_exit() has been called. */
bool android_app_exit_requested(void);
