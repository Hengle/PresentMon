@echo off
if not exist "%~1\build\cef.sln" (
    echo error: could not find %~1\build\cef.sln
    echo usage: pull-cef.bat cef_dir
    exit /b 1
)

set destination=%~dp0..\Cef
echo %destination%

echo Pull in binaries...
xcopy /SY /exclude:%~dp0xcopy-ignore-bin.txt "%~1\Release" "%destination%\Bin\"

echo Pull in libraries...
xcopy /SY "%~1\build\libcef_dll_wrapper\Release" "%destination%\Lib\Release\"
xcopy /SY "%~1\Release\libcef.lib" "%destination%\Lib\Release\"
xcopy /SY "%~1\build\libcef_dll_wrapper\Debug" "%destination%\Lib\Debug\"
xcopy /SY "%~1\Release\libcef.lib" "%destination%\Lib\Debug\"

echo Pull in includes...
xcopy /SY "%~1\include" "%destination%\Include\include\"

echo Pull in resources...
xcopy /SY "%~1\Resources" "%destination%\Resources\"

echo Remove unused locales
for %%i in ("%destination%\Resources\locales\*") do if not %%~nxi == en-US.pak del %%i