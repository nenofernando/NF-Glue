; Inno Setup script for NF Glue (Windows x64)
; Built by Installer\Windows\build_windows_installer.ps1, which passes the version from CMakeLists.txt
; (/DMyAppVersion=X.Y.Z) so there is nothing to edit here on a version bump.
; Expects Installer\Windows\payload\NF Glue.vst3 and, unless built with SKIP_AAX,
; Installer\Windows\payload\NF Glue.aaxplugin (already PACE-signed with wraptool).

#ifndef MyAppVersion
  #error "Build with /DMyAppVersion=X.Y.Z (use build_windows_installer.ps1)"
#endif
#define MyAppName "NF Glue"
#define MyAppPublisher "NF Audio Tools"

[Setup]
AppId={{6F2C1B7A-4D3E-4A8B-9C15-7E0D2A5B8F31}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commonpf64}\NF Audio Tools\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..
OutputBaseFilename={#MyAppName} {#MyAppVersion} Setup
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableDirPage=yes
DisableReadyPage=yes
UninstallDisplayIcon={uninstallexe}
WizardStyle=modern

#ifdef WITH_AAX
[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plug-in"; Types: full custom; Flags: fixed
Name: "aax"; Description: "AAX Plug-in (Pro Tools)"; Types: full
#endif

[Files]
#ifdef WITH_AAX
Source: "payload\NF Glue.vst3\*"; DestDir: "{commoncf64}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "payload\NF Glue.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\{#MyAppName}.aaxplugin"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: aax
#else
Source: "payload\NF Glue.vst3\*"; DestDir: "{commoncf64}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#MyAppName}.vst3"
#ifdef WITH_AAX
Type: filesandordirs; Name: "{commoncf64}\Avid\Audio\Plug-Ins\{#MyAppName}.aaxplugin"
#endif
