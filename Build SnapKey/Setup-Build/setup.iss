#define MyAppName "SnapKey 中文版"
#define MyAppVersion "1.2.9"
#define MyAppPublisher "cafali"
#define MyAppURL "https://github.com/cafali/SnapKey"
#define MyAppExeName "SnapKey.exe"
#define User "cafali"
#define Folder "SnapKeyDEV"

[Setup]
AppId={{72AF690F-C35B-4E3F-B82B-8F75A06B960E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={localappdata}\{#MyAppName}
DisableDirPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName=SnapKey 中文版
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
LicenseFile=C:\Users\{#User}\AppData\Local\{#Folder}\LICENSE
OutputDir=C:\Users\{#User}\Desktop
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-Setup
SetupIconFile=Z:\dev\DEV SnapKey\snapkey.ico
SolidCompression=yes
WizardStyle=classic
WizardImageFile=Z:\dev\DEV SnapKey\wizard_large.bmp
WizardSmallImageFile=Z:\dev\DEV SnapKey\wizard_small.bmp
VersionInfoVersion={#MyAppVersion}


[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加图标："; Flags: unchecked

[Files]
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion  
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\config.cfg"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\icon_off.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\SnapKey.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\{#user}\AppData\Local\{#Folder}\meta\*"; DestDir: "{app}\meta"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{group}\卸载 SnapKey"; Filename: "{uninstallexe}"


[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 SnapKey"; Flags: nowait postinstall skipifsilent
