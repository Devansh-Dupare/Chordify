; Inno Setup script for the Windows installer: Chordify-<version>-Windows.exe
; Build it with packaging\build_windows_installer.bat, which builds the plug-in first.
;
; The build folder defaults to ..\build-installer; override with /DBuildDir=<path> on the iscc
; command line. Signing runs only when /DSign is passed and a "signtool" sign tool is configured
; (see build_windows_installer.bat).

#define Version Trim(FileRead(FileOpen("..\VERSION")))
#define ProductName "Chordify"
#define Publisher "Duphon"
#define Year GetDateTimeString("yyyy","","")
#ifndef BuildDir
  #define BuildDir "..\build-installer"
#endif
#define Artefacts BuildDir + "\Chordify_artefacts\Release"

[Setup]
; Never change AppId: Windows uses it to recognise newer versions as upgrades of this install
AppId={{C0B503B4-79E6-49F3-A043-A8835F51AB31}
AppName={#ProductName}
AppVersion={#Version}
AppVerName={#ProductName} {#Version}
AppPublisher={#Publisher}
AppCopyright=Copyright (C) {#Year} {#Publisher}
OutputBaseFilename={#ProductName}-{#Version}-Windows
OutputDir=Output
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
DefaultDirName={commonpf64}\{#Publisher}\{#ProductName}
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallFilesDir={app}\uninstall
UninstallDisplayIcon={app}\Chordify.ico
UninstallDisplayName={#ProductName}
SetupIconFile=Chordify.ico
WizardStyle=modern
WizardImageFile=windows\wizard.bmp,windows\wizard@2x.bmp
WizardSmallImageFile=windows\wizard-small.bmp,windows\wizard-small@2x.bmp
; Review the EULA before distributing: it's your licence terms
LicenseFile=resources\EULA
#ifdef Sign
SignTool=signtool
SignedUninstaller=yes
#endif

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Common Files\VST3)"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
; MSVC can leave an .ilk next to the plug-in; don't ship it
Source: "{#Artefacts}\VST3\{#ProductName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#ProductName}.vst3\"; Excludes: "*.ilk"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#Artefacts}\Standalone\{#ProductName}.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "Chordify.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#ProductName}"; Filename: "{app}\{#ProductName}.exe"; Components: standalone

[UninstallDelete]
; The VST3 bundle is a folder; remove it entirely, including anything a host added
Type: filesandordirs; Name: "{commoncf64}\VST3\{#ProductName}.vst3"
