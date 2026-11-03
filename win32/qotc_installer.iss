; Quest of the Companions for Windows: the installer, and the update that
; WinSparkle runs quietly over an install (docs/friend-mode/updates.md).
; The friend-mode-windows workflow builds it from the `make install` folder:
;
;   ISCC.exe -DSRCDIR=<folder> -DQOTC_VERSION=0.8.2 -DQOTC_BUILD=240
;            -DQOTC_APPID=<GUID> -DOUTNAME=<name> qotc_installer.iss
;
; It installs for this user only, with no administrator prompt, so updates
; need none either.  Saves and settings live in %LOCALAPPDATA%\Exult, apart
; from the install; the game data players copy into blackgate\ and
; serpentisle\ is theirs and stays through updates and uninstalling.

#ifndef SRCDIR
  #error Pass -DSRCDIR=<the make install folder>
#endif
#ifndef QOTC_VERSION
  #error Pass -DQOTC_VERSION
#endif
#ifndef QOTC_BUILD
  #error Pass -DQOTC_BUILD
#endif
#ifndef QOTC_APPID
  #error Pass -DQOTC_APPID=<GUID> (ours: tools/friend-mode/qotc-release.env)
#endif
#ifndef OUTNAME
  #define OUTNAME "QOTC-Setup"
#endif

[Setup]
; Never change AppId: it's how an update finds the install it replaces.
; "{{" is a literal brace: AppId is {<GUID>}.
AppId={{{#QOTC_APPID}}
AppName=Quest of the Companions
AppVersion={#QOTC_VERSION} (build {#QOTC_BUILD})
AppVerName=Quest of the Companions {#QOTC_VERSION}
AppPublisher=XCAST3D
AppPublisherURL=https://xcast3d.com/u7-quest-of-the-companions/
AppSupportURL=https://xcast3d.com/u7-quest-of-the-companions/
VersionInfoVersion={#QOTC_VERSION}.{#QOTC_BUILD}
DefaultDirName={autopf}\Quest of the Companions
DefaultGroupName=Quest of the Companions
DisableProgramGroupPage=yes
; An update goes where the player installed it (the default, kept explicit).
UsePreviousAppDir=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; The game is closed by InitializeSetup below, not Windows' Restart Manager.
CloseApplications=no
SetupIconFile=exult.ico
UninstallDisplayIcon={app}\Exult.exe
OutputBaseFilename={#OUTNAME}
OutputDir=.
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Messages]
FinishedLabel=Quest of the Companions is installed.%n%nBefore you play, copy the STATIC folder of your own Ultima VII: The Black Gate into the blackgate folder there (Serpent Isle's into serpentisle). Your saves and settings are kept separately, in %LOCALAPPDATA%\Exult.

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Dirs]
Name: "{app}\blackgate"
Name: "{app}\serpentisle"

[Files]
Source: "{#SRCDIR}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Quest of the Companions"; Filename: "{app}\Exult.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Quest of the Companions"; Filename: "{app}\Exult.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Run silently too, so the game starts again after an update.
Filename: "{app}\Exult.exe"; WorkingDir: "{app}"; Description: "Start Quest of the Companions"; Flags: nowait postinstall
Filename: "{app}"; Description: "Open the install folder, to add the game data"; Flags: shellexec postinstall skipifsilent unchecked

[Code]
const
  // Held by the running game (win32/qotcupdates.cc).
  RunningMutex = 'QuestOfTheCompanionsRunning';

function InitializeSetup(): Boolean;
var
  Waited: Integer;
begin
  Result := True;
  // An update starts while the game is still closing (the player may be
  // answering "really quit?"): give it a minute.
  if WizardSilent() then
  begin
    Waited := 0;
    while CheckForMutexes(RunningMutex) and (Waited < 60000) do
    begin
      Sleep(500);
      Waited := Waited + 500;
    end;
  end;
  while CheckForMutexes(RunningMutex) do
  begin
    if MsgBox('Quest of the Companions is running. Close it, then choose Retry.',
        mbError, MB_RETRYCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;
