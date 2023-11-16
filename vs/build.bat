@echo off
setlocal enabledelayedexpansion

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86

msbuild "libwpd.vcxproj" /p:Configuration=Debug /p:Platform=x86
copy "x86\Debug\libwpd.dll" libwpd_32.dll

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

msbuild "libwpd.vcxproj" /p:Configuration=Debug /p:Platform=x64
copy "x64\Debug\libwpd.dll" libwpd_64.dll

endlocal
