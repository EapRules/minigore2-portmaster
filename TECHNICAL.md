# Technical deep dive

**How a 2012 Android NativeActivity game runs natively on ARM Linux — no
emulator, no Android runtime.**

The reference device is the R36S, but nothing here is device-specific: the
techniques apply to any armhf-capable ARM Linux system with 32-bit GPU
libraries, and to the whole class of `android_native_app_glue` games whose
source is gone.

## The execution model

Minigore 2 ships a single `armeabi-v7a` library and runs its own main loop:
it imports `libandroid.so`, expects an `ALooper` with a poll source, an
`AInputQueue`, an `ANativeWindow` and a working JNI environment, and drives
itself. The port maps the library with a bionic-compatible ELF loader
(gmloader-next lineage) and hand-implements the Android surface around it.

Two principles run through every layer:

- **Refuse to fake a load.** The loader resolves *every* import of
  `libminigore2.so` or does not start. An unresolved symbol is a build
  error, not a runtime surprise.
- **Answer truthfully or crash honestly.** The engine dereferences what it
  is given without checking. Audio "politely declined" means a null engine
  pointer dereferenced later, far from the cause. The engine's Java
  callbacks (`xtPlay` among them) are not optional decoration — they are
  load-bearing, and the port implements them for real.

## ABI: the 12 bytes that stop a thread

The game's render thread never started: bionic's `pthread_attr_t` is 12
bytes smaller than glibc's. An attribute block the game allocates on its
stack is complete by bionic's layout and truncated by glibc's — the thread
creation reads past the game's buffer into whatever follows. The thunk
layer translates the structure instead of passing it through. This is the
signature failure mode of the whole port: ABI struct disagreements between
two libcs do not fail at the boundary; they corrupt state and fail
somewhere else.

## Verification that cannot be fooled

The qemu-arm + llvmpipe harness asserts startup milestones — load, GL
context, shaders, assets, frames. Getting it *trustworthy* took three
hardenings, each closing a way the harness lied:

- a `glClear` to a solid colour was passing as "the game renders" — the
  milestone now requires content only a running engine can produce;
- 120 presented frames and 120 *black* frames log identically — the
  harness reads the pixels back;
- a frame counter can advance while the game is frozen — a
  characterised freeze (three hypotheses ruled out on the way) is reported
  as exactly that: `stopped presenting after N frames`.

The harness also has documented false results in both directions (a freeze
that existed only under software rendering; a framebuffer change that
passed under llvmpipe and was black on Mali). It makes iteration possible;
hardware remains the arbiter.

## Input: a touch-only game on a device with no touchscreen

The game is two virtual thumbsticks and touch-only menus. The port reads
the pad natively through `SDL_GameController` and synthesizes the touches
itself:

- sticks become the two thumbs, placed off the HUD and kept apart from the
  menu cursor;
- the D-pad drives an on-screen cursor for the menus — KMS/DRM has no
  hardware cursor plane the port could borrow, so the cursor is drawn into
  the frame just before it is presented, and fades when idle;
- fire is mapped to the button the player *reads* as A: SDL names buttons
  by position while device silkscreens disagree (Nintendo vs Xbox layout),
  so the mapping is a one-line configuration, not a recompile;
- `gptokeyb` still runs (PortMaster expects it by binary name) but every
  binding is deliberately empty — the port already delivers pad input
  natively, and a second keyboard-shaped path would make the game see
  every press twice.

## Packaging for hostile filesystems

The SD card is exFAT: no symlinks, no execute bits to rely on. Anything
that needs to be a real ELF with links — the GL shim among them — is built
under `/tmp` at runtime and cleaned up on exit. The bundled libraries are
collected by an allowlist that is deliberately picky: glibc, the dynamic
linker and anything GPU-related must come from the device, never from the
zip. The loader forces the device's 32-bit Mali blob and says so in the
log; a CFW without 32-bit GPU libraries gets a clear refusal
(`GL: no Mali blob found`) instead of a black screen.

Failure modes are surfaced where the user is: a missing APK is reported on
screen with the exact expected path, the log is written unbuffered straight
to a file (no `tee`, no `stdbuf` — both demonstrably lose the FATAL block
under PortMaster's process-killing exit path), and stubs for ads, IAP and
leaderboards make the game behave as fully unlocked and offline.

None of this is specific to zombies. The loader, the truthful-JNI rule, the
pixel-readback verification, the native controller path and the
exFAT-survival packaging form a repeatable path for the rest of this
generation of Android-native games.
