:: Copyright (C) 2022 Intel Corporation
:: SPDX-License-Identifier: MIT
@echo off
if "%~1"=="" goto usage
if "%~2"=="" goto usage
if not exist %1 goto usage
goto ok
:usage
    echo usage: generate_options_header.cmd path_to_readme.md path_to_output_header.h
    exit /b 1
:ok

where awk > NUL
if %errorlevel% neq 0 (
    echo error: dependency missing: awk
    exit /b 1
)

where sed > NUL
if %errorlevel% neq 0 (
    echo error: dependency missing: sed
    exit /b 1
)

if exist %2 echo.>%2

setlocal enabledelayedexpansion
set done=0
for /f "tokens=1,2 delims=|" %%a in ('sed -nr "/^\|[^|]*\|[^|]*\|$/p" %1') do (
    call :process "%%a" "%%b" %2
    if !done! neq 0 exit /b 0
)

exit /b 0

:process
    set a1=%1
    set a2=%2
    set a1=%a1:`=%
    set a2=%a2:`=%

    set c1=""
    set c2=""
    for /f "tokens=*" %%a in ('echo^| set /p^=%a1% ^| awk "{$1=$1};1"') do set c1="%%a"
    for /f "tokens=*" %%a in ('echo^| set /p^=%a2% ^| awk "{$1=$1};1"') do set c2="%%a"

    if %c1% equ "Column Header" (
        set done=1
        exit /b 0
    )

    if %c1:-=% neq "" (
        if %c2% equ "" (
            echo.>> %3
            echo %c1%, nullptr,>> %3
        ) else (
            echo %c1%, R"(%c2:~1,-1%)",>> %3
        )
    )

    exit /b 0

