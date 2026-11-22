; AniAttach for Daz Studio 6 - Windows installer (Inno Setup 6).
;
; Built by .github/workflows/release.yml:
;   ISCC /DAppVersion=1.0.0 /DDllPath=..\build\Release\dsp_aniattach.dll /DOutputDir=..\dist packaging\aniattach.iss
;
; Installs dsp_aniattach.dll into <Daz Studio 6>\plugins. Finds Daz Studio 6 (and
; the Public Build) through the registry keys Daz Studio writes, lets the user
; pick another folder, and checks that the folder really contains Daz Studio.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef DllPath
  #define DllPath "..\build\Release\dsp_aniattach.dll"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
AppId={{39DD9A94-30A1-4219-A583-68F3B9C8D0E3}
AppName=AniAttach for Daz Studio 6
AppVersion={#AppVersion}
AppVerName=AniAttach {#AppVersion} for Daz Studio 6
AppPublisher=AniAttach contributors
AppPublisherURL=https://github.com/jakeuncles/AniAttach
AppSupportURL=https://github.com/jakeuncles/AniAttach/issues
AppUpdatesURL=https://github.com/jakeuncles/AniAttach/releases
VersionInfoVersion={#AppVersion}
; The "install folder" is the Daz Studio 6 folder; the plugin goes in its plugins folder.
DefaultDirName={code:DefaultDazDir}
AppendDefaultDirName=no
DirExistsWarning=no
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
; Keep the uninstaller out of the Daz Studio folder.
UninstallFilesDir={commonappdata}\AniAttach
UninstallDisplayName=AniAttach {#AppVersion} for Daz Studio 6
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Daz Studio has the plugin loaded while it runs: ask to close it first.
CloseApplications=yes
RestartApplications=no
LicenseFile=..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=AniAttach-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Messages]
SelectDirDesc=Which Daz Studio 6 should AniAttach be installed into?
SelectDirLabel3=AniAttach will be installed into the plugins folder of this Daz Studio 6 installation.
SelectDirBrowseLabel=This is the folder that contains DAZStudio.exe. To use a different Daz Studio 6 installation, click Browse.

[Tasks]
Name: publicbuild; Description: "Also install into Daz Studio 6 Public Build"; Check: HasPublicBuild

[Files]
Source: "{#DllPath}"; DestDir: "{app}\plugins"; Flags: ignoreversion
Source: "{#DllPath}"; DestDir: "{code:PublicBuildDir}\plugins"; Tasks: publicbuild; Flags: ignoreversion
Source: "INSTALL.txt"; DestDir: "{commonappdata}\AniAttach"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{commonappdata}\AniAttach"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{commonappdata}\AniAttach"; Flags: ignoreversion

[Code]
// Daz Studio stores its folder (with forward slashes) under
// Software\DAZ\<edition>\InstallPath-64, per user.
function DazDirFromRegistry(const Edition: String): String;
begin
  Result := '';
  if not RegQueryStringValue(HKCU, 'Software\DAZ\' + Edition, 'InstallPath-64', Result) then
    if not RegQueryStringValue(HKLM64, 'Software\DAZ\' + Edition, 'InstallPath-64', Result) then
      Result := '';
  StringChangeEx(Result, '/', '\', True);
  Result := RemoveBackslashUnlessRoot(Result);
end;

function IsDazFolder(const Dir: String): Boolean;
begin
  Result := (Dir <> '') and FileExists(AddBackslash(Dir) + 'DAZStudio.exe');
end;

function DefaultDazDir(Param: String): String;
begin
  Result := DazDirFromRegistry('Studio6');
  if not IsDazFolder(Result) then
    Result := ExpandConstant('{commonpf64}\DAZ 3D\DAZStudio6');
end;

function PublicBuildDir(Param: String): String;
begin
  Result := DazDirFromRegistry('Studio6PublicBuild');
end;

function HasPublicBuild: Boolean;
begin
  Result := IsDazFolder(PublicBuildDir('')) and
    (CompareText(PublicBuildDir(''), WizardDirValue) <> 0);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and not IsDazFolder(WizardDirValue) then
  begin
    MsgBox('This folder does not contain DAZStudio.exe:' + #13#10 + WizardDirValue + #13#10#13#10 +
      'Choose the folder Daz Studio 6 is installed in, for example' + #13#10 +
      ExpandConstant('{commonpf64}') + '\DAZ 3D\DAZStudio6', mbError, MB_OK);
    Result := False;
  end;
end;
