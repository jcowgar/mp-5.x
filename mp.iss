; InnoSetup file

[Setup]
AppName=Minimum Profit
AppVerName=Minimum Profit version 5.x
DefaultDirName={pf}\mp-5
UsePreviousAppDir=no
DefaultGroupName=Minimum Profit
UninstallDisplayIcon={app}\wmp.exe
Compression=lzma
SolidCompression=yes
; OutputDir=userdocs:Inno Setup Examples Output

[Files]
Source: "wmp.exe"; DestDir: "{app}"
Source: "mp_*.mpsl"; DestDir: "{app}"
Source: "lang\*.mpsl"; DestDir: "{app}\lang"
Source: "doc\*.html"; DestDir: "{app}\doc"
Source: "README" ; DestDir: "{app}\doc"
Source: "AUTHORS" ; DestDir: "{app}\doc"
Source: "COPYING" ; DestDir: "{app}\doc"
Source: "RELEASE_NOTES" ; DestDir: "{app}\doc"
Source: "mp_templates.sample" ; DestDir: "{app}\doc"
Source: "TODO" ; DestDir: "{app}\doc"
Source: "mp.reg" ; DestDir: "{app}\doc"

[Icons]
Name: "{group}\Minimum Profit"; Filename: "{app}\wmp.exe"
