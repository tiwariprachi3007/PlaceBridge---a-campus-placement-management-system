#define AppName "PlaceBridge"
#define AppVersion "2.0"
#define AppExeName "PlaceBridge.exe"

[Setup]
AppId={{BB280B78-4364-4B3D-A20E-1A2B3C4D0001}
AppName={#AppName}
AppVersion={#AppVersion}
DefaultDirName={autopf}\PlaceBridge
DefaultGroupName=PlaceBridge
OutputDir=output
OutputBaseFilename=PlaceBridge-Setup
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest

[Files]
Source: "..\PlaceBridge-standalone\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\PlaceBridge"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\PlaceBridge"; Filename: "{app}\{#AppExeName}"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch PlaceBridge"; Flags: postinstall nowait skipifsilent
