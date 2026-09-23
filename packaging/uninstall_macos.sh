#!/usr/bin/env bash
# Removes everything the Chordify installer put on this Mac, and the installer's receipts.
# Double-click "Uninstall Chordify.command", or run it in Terminal. Asks for an admin password.

set -uo pipefail

FILES=(
    "/Library/Audio/Plug-Ins/VST3/Chordify.vst3"
    "/Library/Audio/Plug-Ins/Components/Chordify.component"
    "/Applications/Chordify.app"
)
RECEIPTS=(com.duphon.chordify.vst3.pkg com.duphon.chordify.au.pkg com.duphon.chordify.app.pkg)

echo "This removes Chordify from:"
printf '  %s\n' "${FILES[@]}"
read -r -p "Continue? [y/N] " answer
[[ "$answer" =~ ^[Yy]$ ]] || { echo "Nothing removed."; exit 0; }

for path in "${FILES[@]}"; do
    if [ -e "$path" ]; then
        sudo rm -rf "$path" && echo "removed $path"
    fi
done

for receipt in "${RECEIPTS[@]}"; do
    pkgutil --pkg-info "$receipt" > /dev/null 2>&1 && sudo pkgutil --forget "$receipt" > /dev/null && echo "forgot receipt $receipt"
done

# Copies in your own user Library weren't made by the installer, so they're left alone
for path in ~/Library/Audio/Plug-Ins/VST3/Chordify.vst3 ~/Library/Audio/Plug-Ins/Components/Chordify.component; do
    [ -e "$path" ] && echo "note: $path is still there (not installed by the installer)"
done

echo "Chordify has been uninstalled."
