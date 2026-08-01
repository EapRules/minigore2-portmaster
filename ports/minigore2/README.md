# Minigore 2: Zombies — PortMaster port

Twin-stick zombie shooter by Mountain Sheep (2012). This is **not an emulator**:
the game's own Android native library is loaded and run directly on Linux/ARM
through a NativeActivity loader, the same way GMLoader runs GameMaker games.

## After installing: reboot, and stay out of Manage Ports

Two things that make a working install look broken. Both cost hours to work out,
so they go first:

**The game does not appear in Ports until you reboot the console.** PortMaster
installs it correctly and then says nothing: the autoinstall path never triggers
the frontend refresh, so EmulationStation keeps listing what it loaded at boot.
Reboot and it is there.

**Do not press *Reinstall* or *Uninstall* under Manage Ports.** Both re-download
from PortMaster's catalogue, where this port does not exist, so you get *"unable
to find a source for minigore2.zip"* — after the port has already been removed.
That turns a good install into an empty Ports menu, and it is a very easy thing
to do twice while trying to fix the first one. To reinstall, drop the zip into
`autoinstall/` again. *Uninstall* additionally deletes the port folder with
**your APK inside it**.

## You need to supply the game

The port carries only the loader. The game itself is yours to provide:

1. Get the Android APK for **`net.mountainsheep.minigore2zombies`**, **version
   1.28** — the last release, from May 2017. The game is no longer on Google Play.
2. Rename it to **`minigore2.apk`**.
3. Put it in **`ports/minigore2/minigore2.apk`** in the same session in which you
   drop the zip into `autoinstall/`, while the card is still in your computer.

Nothing is downloaded and no game data ships in this zip.

## Requirements

- **armhf userland with 32-bit GPU libraries.** The game ships only an
  `armeabi-v7a` library and no 64-bit build exists, so the loader is 32-bit.
  Your CFW needs `CONFIG_COMPAT` in the kernel and 32-bit Mali libraries — the
  same requirement as box86 and GMLoader. Devices without 32-bit GPU libraries
  (for example the TrimUI Smart Pro) cannot run this.
- **glibc 2.38 or newer.** The binary imports symbols up to `GLIBC_2.38`.

Tested on: R36S (G80CA-MB V1.2, RK3326, Mali-G31) running dArkOSRE. That is the
only device it has run on.

## Controls

This build of the game is touch-only: two virtual thumbs during play, and menus
that are plain touch targets with no gamepad navigation at all. The port turns
the pad into those touches.

| Control | Does |
|---|---|
| Left stick | walk |
| Right stick | aim by hand |
| **A** (right-hand button), **R1** or **R2** | **fire** |
| **D-pad** | **move the on-screen cursor** |
| **B** (bottom button) | **tap where the cursor is** — menus only |
| **Start** | **open the game's menu** (Android BACK) |
| Select, L1, L2, X, Y | passed through as Android gamepad keys |

Fire sits on three controls because they suit different hands, and holding any
of them keeps firing — releasing one while another is held does not stop it.

The game ships with AUTO-AIM on, so the right stick is optional: it is not
really aiming, it is saying "shoot", and a button says that better. The fire
button and the right stick share one finger — the button decides whether it is
down, the stick decides where it points. Hold fire alone and auto-aim picks the
target; nudge the stick to aim by hand.

**B does nothing while playing**, on purpose: a thumb resting on it must never
poke the screen mid-fight.

### If fire is on the wrong button

SDL names the face buttons by **position** — its "A" is always the bottom one —
while handhelds letter them however they like. These devices are lettered
Nintendo style, with A on the right, and the port assumes that.

If your handheld is lettered Xbox style (A at the bottom), fire will land on the
wrong button. Edit `Minigore 2.sh` and change one line:

```sh
export MINIGORE_FACE_LAYOUT="${MINIGORE_FACE_LAYOUT:-xbox}"
```

Fire and the menu-tap swap places; nothing else changes.

Sticks and cursor are separate modes and never overlap: touching a stick
retires the cursor immediately, so a button press while playing can never poke
the screen. The cursor also fades on its own after a few idle seconds.

The cursor is what gets you through the menus. It only appears once you move
the d-pad, and it rides the d-pad rather than a stick so that both sticks stay
free while playing. KMS/DRM has no hardware cursor, so the port draws it
itself, over the game, just before each frame is presented.

`minigore2.gptk` leaves every button unbound on purpose. gptokeyb is present
only so PortMaster's standard exit combination can close the game — binding
buttons there would double every input.

## If it does not start

The port writes `ports/minigore2/log.txt` on every run. Useful lines:

| In the log | Meaning |
|---|---|
| `missing game file` on screen | the APK is not in place, see above |
| `GL: no Mali blob found` | the port could not find 32-bit Mali libraries |
| `unresolved symbol` | the loader is missing a shim; please report it |
| `stopped presenting after N frames` | the game froze; N is useful, include it |

## Known issues

- **The settings menu overlaps itself** — the game lays it out for 16:9 and the
  panel is 4:3. Navigable, just ugly. Rendering to a 16:9 buffer was tried and
  gives a black screen on Mali, so it was reverted.
- Ads, in-app purchases, leaderboards and cloud saves are stubbed out. The game
  behaves as if everything is unlocked and there is no network.

## Credits

Game by **Mountain Sheep**. This port bundles no game assets and does not
circumvent any protection: it loads an APK the user already owns.

The ELF loader, the JNI shim and the bionic libc thunks derive from
[gmloader-next](https://github.com/JohnnyonFlame/gmloader-next) by JohnnyonFlame
— see `LICENSE-gmloader.md`.
