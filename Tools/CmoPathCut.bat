rem FBXのフォルダ
set FBX=..\FBX

rem CMOの出力フォルダ
set CMO=..\CMO

rem COMのリソースフォルダ
set OUT=..\Resources\Models

CmoPathCut.exe /f %FBX% /c %CMO%
xcopy /Y %CMO% %OUT%

