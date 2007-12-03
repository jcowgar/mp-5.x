; InnoSetup file

[Setup]
AppName=Minimum Profit
AppVerName=Minimum Profit version 5.x
DefaultDirName={pf}\mp-5
DefaultGroupName=Minimum Profit
UninstallDisplayIcon={app}\wmp.exe
Compression=lzma
SolidCompression=yes
; OutputDir=userdocs:Inno Setup Examples Output

[Files]
Source: "wmp.exe"; DestDir: "{app}"
Source: "mp_*.mpsl"; DestDir: "{app}"
Source: "doc\*.*"; DestDir: "{app}\doc"
;Source: "Readme.txt"; DestDir: "{app}"; Flags: isreadme

[Icons]
Name: "{group}\Minimum Profit"; Filename: "{app}\wmp.exe"
