; SPDX-License-Identifier: GPL-3.0-or-later
; Command-line defines supplied by GitHub Actions: SourceDir and AppVersion.

#ifndef SourceDir
  #error SourceDir must point to the staged application directory.
#endif

#ifndef AppVersion
  #error AppVersion must contain the application version.
#endif

[Setup]
AppId={{B9A3C3A9-3AE8-466D-B2A4-4C596B04F477}
AppName=VERA Test Console
AppVersion={#AppVersion}
AppPublisher=Open VERA Module
DefaultDirName={autopf}\VERA Test Console
DefaultGroupName=VERA Test Console
DisableProgramGroupPage=yes
OutputBaseFilename=vera-test-console-{#AppVersion}-windows-x86_64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\vera-gui.exe
SetupIconFile={#SourceDir}\vera-gui-icon.ico

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\VERA Test Console"; Filename: "{app}\vera-gui.exe"
Name: "{autodesktop}\VERA Test Console"; Filename: "{app}\vera-gui.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\vera-gui.exe"; Description: "Launch VERA Test Console"; Flags: nowait postinstall skipifsilent
