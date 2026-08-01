#!/usr/bin/env bash
#
# Build the release zip that goes in PortMaster's autoinstall/ folder.
#
# The layout follows PortMaster's packaging guide
# (https://portmaster.games/packaging.html): the launch script sits at the top
# of the zip and everything else lives in a folder named after the port, which
# is what lands in ports/ when PortMaster unpacks it.
#
# The file that is easy to miss is gameinfo.xml. PortMaster uses it to write
# the entry into EmulationStation's gamelist.xml on install - name, artwork,
# description. Without it the port installs correctly, PortMaster even lists it
# under Manage Ports, and the game still never appears in the Ports menu,
# because the frontend was never told about it. That failure looks exactly like
# a broken port and is not one.
#
# licenses/ is likewise required by the guide: every borrowed source, library
# and asset gets its licence shipped alongside the binary. The loader here is
# GPL, so this is an obligation, not a formality.
#
# What never travels is the game: no APK, no assets from it. The cover and the
# optional 4:3 splash are promotional artwork, not game data - see NOTICE.md.

set -euo pipefail

cd "$(dirname "$0")/.."

OUT="build/minigore2.zip"
STAGE="build/pkg"

[ -x build/minigore2 ] || { echo "build/minigore2 missing - run make first" >&2; exit 1; }
[ -d build/libs.armhf ] || { echo "build/libs.armhf missing - run tools/collect_libs.sh" >&2; exit 1; }

rm -rf "$STAGE" "$OUT"
mkdir -p "$STAGE/minigore2"

cp "ports/Minigore 2.sh"           "$STAGE/"
cp build/minigore2                 "$STAGE/minigore2/"
cp ports/minigore2/minigore2.gptk  "$STAGE/minigore2/"
cp ports/minigore2/port.json       "$STAGE/minigore2/"
cp ports/minigore2/gameinfo.xml    "$STAGE/minigore2/"
cp ports/minigore2/cover.png       "$STAGE/minigore2/"
cp ports/minigore2/screenshot.png  "$STAGE/minigore2/"
cp ports/minigore2/README.md       "$STAGE/minigore2/"
cp -R ports/minigore2/licenses     "$STAGE/minigore2/"
cp -R build/libs.armhf             "$STAGE/minigore2/"
cp -R ports/minigore2/assets       "$STAGE/minigore2/"

chmod +x "$STAGE/Minigore 2.sh" "$STAGE/minigore2/minigore2"

# macOS sprinkles ._* resource forks over anything it touches, and they end up
# in the zip looking like broken duplicates of every file.
if command -v dot_clean >/dev/null 2>&1; then
    dot_clean -m "$STAGE"
fi
find "$STAGE" \( -name '._*' -o -name '.DS_Store' \) -delete

( cd "$STAGE" && zip -qr "../../$OUT" . -x '.*' )

# Fail loudly rather than shipping a zip that installs but never shows up.
#
# The listing is captured once instead of being piped per check: under
# `set -o pipefail`, grep -q exits as soon as it matches, unzip gets SIGPIPE,
# and the pipeline reports failure for a file that is present. Every check
# would then "fail" on a perfectly good zip.
listing="$(unzip -l "$OUT")"
for required in "Minigore 2.sh" minigore2/port.json minigore2/gameinfo.xml \
                minigore2/cover.png minigore2/screenshot.png minigore2/minigore2; do
    case "$listing" in
        *"$required"*) ;;
        *) echo "MISSING FROM ZIP: $required" >&2; exit 1 ;;
    esac
done

echo "$OUT"
unzip -l "$OUT" | tail -3
