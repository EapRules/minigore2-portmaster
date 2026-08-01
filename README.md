# Minigore 2: Zombies — native port for Linux ARM handhelds

Runs **Minigore 2: Zombies** (Mountain Sheep, 2012) natively on the R36S and
similar handhelds. **No emulator and no Android runtime**: the game's own
`armeabi-v7a` library is loaded straight into a Linux process by a bionic ELF
loader, and the pieces of Android it asks for are implemented here by hand.

> **Bring your own game.** This repository and its releases contain **zero game
> content**. You supply an APK you own; nothing is downloaded and no protection
> is circumvented.

Download the ready-to-use zip from the [Releases](../../releases) page.

**How does a 2012 Android binary run natively on ARM Linux?** The loader,
the truthful-JNI rule, the pixel-readback verification and the packaging
for exFAT are documented in [`TECHNICAL.md`](TECHNICAL.md).

## Install

**Do not unzip this into `ports/` by hand.** PortMaster's own FAQ warns that
doing so can leave a port that never starts — it is how you end up without the
execute bit and without the menu entry. Let PortMaster install it:

1. **With the card in your computer**, drop `minigore2.zip` into PortMaster's
   `autoinstall/` folder, unchanged:

   | CFW | Folder |
   |---|---|
   | ArkOS, dArkOS | `/roms/tools/PortMaster/autoinstall/` |
   | AmberELEC, ROCKNIX, JELOS, uOS | `/roms/ports/PortMaster/autoinstall/` |
   | muOS | `/mmc/MUOS/PortMaster/autoinstall/` |
   | Knulli | `/userdata/system/.local/share/PortMaster/autoinstall/` |

2. **Put your APK at `ports/minigore2/minigore2.apk`** — same card, same trip,
   before it goes back in the console. Create the folder if it does not exist
   yet; the install will not remove it. That exact name, next to where the
   `minigore2` binary will land:

   ```
   ports/
   ├── Minigore 2.sh
   └── minigore2/
       ├── minigore2            (the loader)
       ├── minigore2.apk        ← yours, you add this
       ├── minigore2.gptk
       ├── libs.armhf/
       └── assets/
   ```

   Watch the extension: some sites hand you `something.5apk` or `.apk.zip`, and
   it has to end in `.apk`.

3. **Put the card back in the console and open PortMaster.** It installs the
   port, sets the permissions and adds the menu entry. Close it when it is done.

4. **Reboot the console.** This step is not optional and it is the one everybody
   skips — see below.

5. **Launch Minigore 2** from the Ports menu.

### Read this before deciding the port is broken

**The game will not appear in Ports until you reboot.** PortMaster installs it
correctly and then says nothing more: the autoinstall path never triggers the
frontend refresh, so EmulationStation keeps showing the list it loaded at boot.
Everything is on the card, it is simply not being listed yet. Reboot and it is
there.

**Do not use *Reinstall* or *Uninstall* under Manage Ports.** Both re-download
from PortMaster's own catalogue, and this port does not live there, so you get
*"unable to find a source for minigore2.zip"* — after the port has already been
removed. That is how a working install turns into an empty Ports menu, and it is
easy to do twice in a row while trying to fix the first one. To reinstall or
update, drop the zip into `autoinstall/` again.

*Uninstall* is also worth avoiding for a second reason: it deletes the whole
port folder, **your APK included**.

**"No internet connection"** right after installing is harmless. PortMaster is
trying to refresh its catalogue and fetch box art from its servers; this port is
not in that catalogue, so there is nothing to fetch — the artwork ships in the
zip. The install already succeeded.

If the APK is missing the port says so on screen, with the path it expects,
instead of dropping you back to the menu with no explanation.

## Controls

The game is touch-only — two virtual thumbs in play, and menus with no gamepad
navigation whatsoever. The port turns the pad into those touches.

| Control | Does |
|---|---|
| Left stick | walk |
| Right stick | aim by hand |
| **A** (right-hand button), **R1** or **R2** | **fire** |
| D-pad | move the on-screen cursor |
| B (bottom button) | tap where the cursor is — menus only |
| Start | the game's menu (Android BACK) |

The cursor is how you get through the menus. It appears when you move the
d-pad, rides the d-pad so both sticks stay free, and fades after a few idle
seconds. KMS/DRM has no hardware cursor, so the port draws it itself just
before each frame is presented.

Auto-aim is on by default, so the right stick is optional: hold **A** and the
game picks the target.

**If fire lands on the wrong button**, your handheld is lettered Xbox style
(A at the bottom) rather than Nintendo style (A on the right). SDL names buttons
by position and the silkscreen disagrees, so set this in `Minigore 2.sh`:

```sh
export MINIGORE_FACE_LAYOUT="${MINIGORE_FACE_LAYOUT:-xbox}"
```

## Requirements

- **armhf userland with 32-bit GPU libraries.** The game ships only an
  `armeabi-v7a` library, so the loader is 32-bit. Your CFW needs `CONFIG_COMPAT`
  in the kernel and 32-bit Mali libraries — the same bar as box86 and GMLoader.
  Devices without them (the TrimUI Smart Pro, for instance) cannot run this.
- **glibc 2.38+.**

Tested on an **R36S** (G80CA-MB V1.2, RK3326, Mali-G31) running dArkOSRE. That
is the only device this has run on, so anything else is unknown rather than
unsupported — reports welcome either way.

## Reporting a problem

The port writes **`ports/minigore2/log.txt`** on every run, and it is the only
diagnostic anyone gets off a handheld. Open an issue with:

- your device and CFW (e.g. "RG40XXH, muOS 2410")
- `log.txt` from the failed run
- what you saw on screen

Lines worth knowing about:

| In the log | Meaning |
|---|---|
| `missing game file` on screen | the APK is not where the port expects it |
| `GL: no Mali blob found` | no 32-bit GPU libraries on this CFW — the port cannot run here |
| `unresolved symbol` | the loader is missing a shim; please report it, it is fixable |
| `stopped presenting after N frames` | the game froze; include N |

## Building

Everything builds in a container, so the only dependency is Docker (or
[colima](https://github.com/abiosoft/colima) on macOS). The image is Debian
arm64 — native and fast on Apple Silicon — cross-compiling to
`arm-linux-gnueabihf`.

```bash
docker build -f Dockerfile.build -t minigore2-build .
docker run --rm -v "$PWD":/src -w /src minigore2-build make -j4
```

The binary lands at `build/minigore2`. To assemble the libraries that travel in
the zip:

```bash
docker run --rm -v "$PWD":/src -w /src minigore2-build tools/collect_libs.sh
```

`collect_libs.sh` is deliberately picky about what ships: glibc, the dynamic
linker and anything GPU-related must come from the device, never from the zip.

## Verifying without the console

`harness/verify.sh` runs the armhf binary under `qemu-arm` with Mesa llvmpipe
and reports which milestone it reaches — loads, starts, gets a GL context,
compiles shaders, opens assets, draws frames. It makes most iteration possible
without touching the SD card.

**It is not a substitute for the device.** It has no joystick, no Mali driver
and no KMSDRM, and it has produced convincing false results in both directions:
a freeze that existed only under software rendering, and a framebuffer change
that passed there and gave a black screen on Mali. Test on hardware as soon as
the game draws anything.

## Known issues

- **The settings menu overlaps itself** — the layout is 16:9 and the panel is
  4:3. Navigable, just ugly.
- Ads, in-app purchases, leaderboards and cloud saves are stubbed: the game
  behaves as if everything is unlocked and there is no network.

## Credits and licence

Game by **Mountain Sheep**. Port by **EapRules**.

The ELF loader, the fake JNI and the bionic libc thunks derive from
[gmloader-next](https://github.com/JohnnyonFlame/gmloader-next) by JohnnyonFlame,
itself descended from Andy Nguyen's Vita so-loader.

Released under **GPL-3.0** — see [`LICENSE`](LICENSE), and [`NOTICE.md`](NOTICE.md)
for the full provenance of every borrowed part.
