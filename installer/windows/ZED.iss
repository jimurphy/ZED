; Compatible with Inno Setup 6.7.x. Compile through build-installer.ps1.
#ifndef BundleSource
  #error BundleSource must name the validated Release ZED.vst3 directory
#endif
#ifndef InstallerOutput
  #error InstallerOutput must name a dedicated generated-output directory
#endif

[Setup]
; Stable upgrade identity: NEVER regenerate for a new ZED version.
AppId={{10FF1E2C-BA87-4F89-9628-59B401DB1B3E}
AppName=ZED
AppVersion=1.0.0
AppVerName=ZED 1.0.0 RC1
AppPublisher=South Coast Synthesis
VersionInfoVersion=1.0.0.0
VersionInfoProductVersion=1.0.0
UninstallDisplayName=ZED 1.0.0 RC1
DefaultDirName={autopf}\South Coast Synthesis\ZED
UninstallFilesDir={app}
UsePreviousAppDir=no
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0
OutputDir={#InstallerOutput}
OutputBaseFilename=ZED-1.0.0-rc.1-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
AlwaysRestart=no
InfoAfterFile=Finished.txt
Uninstallable=yes

[Messages]
WelcomeLabel2=This will install ZED 1.0.0 RC1 for all users.%n%nSave your work and close all DAWs and plug-in hosts before continuing. After installation, restart your DAW and rescan VST3 plug-ins if needed.

[Files]
Source: "{#BundleSource}\*"; DestDir: "{commoncf64}\VST3\ZED.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

; No shortcuts, VST registration, launch actions, or wildcard deletion rules.
; Inno Setup records installed files and removes only those on uninstall.
