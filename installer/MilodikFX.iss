; Inno Setup script for MilodikFX.
;
; The application is a single self-contained executable: the UI bundle is baked
; into it and the C runtime is linked statically, so there is nothing else to
; install and no redistributable to chase.
;
; Build with:  iscc /DMyAppVersion=0.9.0 installer\MilodikFX.iss

#ifndef MyAppVersion
  #define MyAppVersion "0.9.0"
#endif

#ifndef MyAppSource
  #define MyAppSource "..\build\MilodikFX_artefacts\Release\MilodikFX.exe"
#endif

#define MyAppName "MilodikFX"
#define MyAppPublisher "MilodikFX"
#define MyAppExeName "MilodikFX.exe"

[Setup]
AppId={{7C4E0A2B-9F3D-4E51-8A6C-2D5B1E9F4A73}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=MilodikFX-{#MyAppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\resources\icon.ico
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "{#MyAppSource}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Leave the user's presets in Documents alone; only remove what the app caches.
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\WebView2"

[Code]
function IsWebView2RuntimeInstalled: Boolean;
var
  Version: String;
begin
  // The UI renders in the Edge WebView2 runtime. It ships with Windows 11 and
  // recent Windows 10, but check rather than let the window come up blank.
  Result :=
    RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', Version) or
    RegQueryStringValue(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', Version) or
    RegQueryStringValue(HKCU, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', Version);
end;

function InitializeSetup: Boolean;
begin
  Result := True;

  if not IsWebView2RuntimeInstalled then
  begin
    if MsgBox('MilodikFX menggunakan Microsoft Edge WebView2 Runtime untuk tampilannya, '
              + 'dan runtime itu tidak terdeteksi di komputer ini.' + #13#10#13#10
              + 'Instalasi tetap bisa dilanjutkan, tetapi jendela aplikasi akan kosong '
              + 'sampai WebView2 Runtime dipasang dari microsoft.com.' + #13#10#13#10
              + 'Lanjutkan?', mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
  end;
end;
