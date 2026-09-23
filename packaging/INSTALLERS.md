# Logo, icons and installers

## Artwork

Every logo and icon is drawn by `source/ui/Logo.cpp` and exported by the `MakeArt` tool, so they
can't drift apart. After changing the logo:

```bash
cmake --build cmake-build-debug --target MakeArt
./cmake-build-debug/MakeArt packaging
for f in wizard wizard@2x wizard-small wizard-small@2x; do
    sips -s format bmp packaging/windows/$f.png --out packaging/windows/$f.bmp
done
```

| File | Used for |
| --- | --- |
| `icon.png`, `icon_small.png` | `ICON_BIG` / `ICON_SMALL` in `CMakeLists.txt`: JUCE builds the standalone app's `.icns` (macOS) and `.ico` (Windows) from them |
| `Chordify.icns` | spare macOS icon (e.g. a DMG or a custom Finder icon) |
| `Chordify.ico` | Windows installer and uninstaller icon |
| `resources/background.png`, `background-dark.png` | macOS installer artwork (light / dark mode) |
| `windows/wizard*.bmp` | Inno Setup wizard images (100% and 200%) |
| `marketing/` | 2048 px icon and a transparent wordmark for a website or store |

The editor draws the same logo live in its branding area.

## macOS installer

```bash
packaging/build_macos_installer.sh
```

Builds a Universal (Apple Silicon + Intel) Release and writes
`packaging/Output/Chordify-<version>-macOS.pkg` plus `Uninstall Chordify.command`. The installer
offers VST3 (`/Library/Audio/Plug-Ins/VST3`), AU (`/Library/Audio/Plug-Ins/Components`) and the
standalone app (`/Applications`) as separate choices. Installing a newer version upgrades in
place. The Release build also copies the plug-ins into `~/Library/Audio/Plug-Ins`
(`COPY_PLUGIN_AFTER_BUILD`), replacing your dev build there.

**Signing and notarization** (needed before giving the installer to anyone else; without it
Gatekeeper blocks or warns):

1. Join the Apple Developer Program and create **Developer ID Application** and **Developer ID
   Installer** certificates (Xcode > Settings > Accounts > Manage Certificates).
2. Store notarization credentials once:
   `xcrun notarytool store-credentials chordify --apple-id <you@example.com> --team-id <TEAMID>`
3. Build with them set:

```bash
DEVELOPER_ID_APPLICATION="Developer ID Application: Your Name (TEAMID)" \
DEVELOPER_ID_INSTALLER="Developer ID Installer: Your Name (TEAMID)" \
NOTARY_PROFILE=chordify \
packaging/build_macos_installer.sh
```

The script signs each bundle with the hardened runtime and JUCE's generated entitlements
(including audio input for the standalone app), signs the `.pkg`, notarizes it and staples the
ticket.

## Windows installer

On a Windows PC with Visual Studio 2022, CMake and Inno Setup 6:

```bat
packaging\build_windows_installer.bat
```

Writes `packaging\Output\Chordify-<version>-Windows.exe`, which installs the VST3 to
`C:\Program Files\Common Files\VST3` and the standalone app to `C:\Program Files\Duphon\Chordify`,
each optional, with an uninstaller. To sign, set `SIGN_COMMAND` to a `signtool` command line (see
the script); Azure Trusted Signing or an Authenticode certificate both work.

**Not yet tested**: these Windows scripts were written on macOS and have never been run. Expect
to fix small things on the first Windows build.

## Before release: test on clean machines

- macOS: install the notarized `.pkg` on a clean user account (Gatekeeper on); check each format
  loads in a DAW and the standalone app asks for microphone access; run the uninstaller and check
  nothing is left in the plug-in folders.
- Windows: install on a clean VM, then install a newer version over it; check SmartScreen doesn't
  warn (signed builds only), the VST3 loads in a DAW, and uninstalling removes everything.
- Review `resources/EULA` (the template's placeholder licence) and `resources/README` before
  shipping.
