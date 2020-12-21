@echo off
rem Do not edit! This batch file is created by CASIO fx-9860G SDK.


if exist SIMPLEXC.G1A  del SIMPLEXC.G1A

cd debug
if exist FXADDINror.bin  del FXADDINror.bin
"C:\casio\OS\SH\Bin\Hmake.exe" Addin.mak
cd ..
if not exist debug\FXADDINror.bin  goto error

"C:\casio\Tools\MakeAddinHeader363.exe" "C:\casioPr\simplexCasio"
if not exist SIMPLEXC.G1A  goto error
echo Build has completed.
goto end

:error
echo Build was not successful.

:end

