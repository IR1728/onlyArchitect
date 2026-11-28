#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

[Setup]
AppId={{8F9D4C2B-6A14-4F0A-9A7C-0A1B2C3D4E5F}
AppName=onlyArchitect Studio
AppVersion={#MyAppVersion}
AppPublisher=onlyArchitect
AppPublisherURL=https://github.com/IR1728/onlyArchitect
LicenseFile=..\..\LICENSE
DefaultDirName={autopf}\onlyArchitect Studio
DefaultGroupName=onlyArchitect Studio
AllowNoIcons=yes
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\..\dist-installers
OutputBaseFilename=onlyArchitect-Setup-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associateqea"; Description: "Asociar archivos .qea con onlyArchitect Studio"; GroupDescription: "Asociaciones de archivo:"

[Files]
Source: "..\..\pkg\onlyArchitect-windows-x64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\onlyArchitect Studio"; Filename: "{app}\onlyarchitect-gui.exe"
Name: "{group}\{cm:UninstallProgram,onlyArchitect Studio}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\onlyArchitect Studio"; Filename: "{app}\onlyarchitect-gui.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.qea"; ValueType: string; ValueName: ""; ValueData: "onlyArchitect.QeaFile"; Flags: uninsdeletevalue; Tasks: associateqea
Root: HKA; Subkey: "Software\Classes\onlyArchitect.QeaFile"; ValueType: string; ValueName: ""; ValueData: "onlyArchitect UML Repository (.qea)"; Flags: uninsdeletekey; Tasks: associateqea
Root: HKA; Subkey: "Software\Classes\onlyArchitect.QeaFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\onlyarchitect-gui.exe,0"; Tasks: associateqea
Root: HKA; Subkey: "Software\Classes\onlyArchitect.QeaFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\onlyarchitect-gui.exe"" ""%1"""; Tasks: associateqea

[Run]
Filename: "{app}\onlyarchitect-gui.exe"; Description: "{cm:LaunchProgram,onlyArchitect Studio}"; Flags: nowait postinstall skipifsilent
