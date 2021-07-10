:: Copyright (C) 2020-2021 Intel Corporation
:: SPDX-License-Identifier: MIT

@echo off
setlocal enabledelayedexpansion
set version=%1
set output_path=%2
set output_dir=%~dp2
set output_dir_short=%~dps2
set git_path=

if not exist %output_dir_short%NUL (
    mkdir "%output_dir%"
    if not !errorlevel!==0 exit /b 1
)

if exist %output_path% del /Q /F %output_path%

if not "%version%"=="dev" goto write_version

for %%a in (git.exe) do set git_path=%%~$PATH:a
if "%git_path%"=="" (
    echo warning: git not found in your PATH, built version will not include latest commit hash
    set version=development branch
    goto write_version
)

for /f %%a in ('"%git_path%" rev-parse HEAD') do set version=development branch %%a

:write_version

echo char const* PRESENT_MON_VERSION = "%version%"; > %output_path%
if not exist %output_path% exit /b 1

exit /b 0
