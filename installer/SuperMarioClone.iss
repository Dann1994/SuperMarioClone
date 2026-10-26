; Instalador de Super Mario Clone (Inno Setup 6). Lo arma tools/publish_release.py (o installer/build_installer.sh), que le
; pasa la version: /DAppVersion=1.0.0. Mismo esquema que el de Battle City.
; Instala solo para el usuario actual (no pide permisos de administrador) en %LOCALAPPDATA%\Programs\SuperMarioClone, crea los
; accesos del menu Inicio (y opcionalmente del escritorio) y registra el desinstalador en "Aplicaciones instaladas".
; Las actualizaciones automaticas del juego descargan este mismo instalador y lo ejecutan en silencio (/SILENT): reemplaza los
; archivos y vuelve a abrir el juego. Las partidas, la configuracion y los niveles propios estan en
; %APPDATA%\SuperMarioClone (no en la carpeta del juego), asi que actualizar o desinstalar no los toca.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppName "Super Mario Clone"
#define AppExe "mario.exe"

[Setup]
AppId={{D36ED7B9-8721-44E4-A938-CE9A71C7DE43}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Dann1994
AppPublisherURL=https://github.com/Dann1994/SuperMarioClone
AppSupportURL=https://github.com/Dann1994/SuperMarioClone/issues
AppUpdatesURL=https://github.com/Dann1994/SuperMarioClone/releases
DefaultDirName={autopf}\SuperMarioClone
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
SetupIconFile=..\resources\icon.ico
OutputDir=..\dist
OutputBaseFilename=SuperMarioClone-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el &escritorio"; GroupDescription: "{cm:AdditionalIcons}"

[InstallDelete]
; los niveles de la campana vienen con cada version: se reemplazan enteros (los propios estan en %APPDATA%)
Type: filesandordirs; Name: "{app}\levels"

[Files]
Source: "..\dist\SuperMarioClone\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\SuperMarioClone\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\SuperMarioClone\LEEME.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\SuperMarioClone\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\SuperMarioClone\levels\*"; DestDir: "{app}\levels"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"
Name: "{autoprograms}\Desinstalar {#AppName}"; Filename: "{uninstallexe}"

[Run]
; en la instalacion normal es una casilla al final; en la actualizacion silenciosa (/SILENT) el juego se vuelve a abrir solo
Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall
