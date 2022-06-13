:: Copyright (C) 2020-2021 Intel Corporation
:: SPDX-License-Identifier: MIT

@echo off
setlocal enabledelayedexpansion
set presentmon=%1
set rootdir=%~2
set force=0
if "%~1" equ "" goto usage
if "%~2" equ "" goto usage
if not exist %presentmon% goto usage
if not exist "%rootdir%\." goto usage
if "%~3"=="force" (
    set force=1
) else (
    if not "%~3"=="" goto usage
)
goto args_ok
:usage
    echo usage: create_gold_csvs.cmd PresentMonPath GoldEtlCsvRootDir [force]
    exit /b 1
:args_ok
set already_exists=0

set pmargs=-no_top -stop_existing_session -qpc_time -track_debug
:: BEGIN internal-only
set pmargs=%pmargs% -track_gpu
set pmargs=%pmargs% -track_gpu_video
set pmargs=%pmargs% -track_input
set pmargs=%pmargs% -track_memory_residency
set pmargs=%pmargs% -track_queue_timers
set pmargs=%pmargs% -track_cpu_gpu_sync
set pmargs=%pmargs% -track_shader_compilation
:: END internal-only
for /f "tokens=*" %%a in ('dir /s /b /a-d "%rootdir%\*.etl"') do call :create_csv "%%a"

if %already_exists% neq 0 echo Use 'force' command line argument to overwrite.
exit /b 0

:create_csv
    if exist "%~dpn1.csv" if %force% neq 1 (
        echo Already exists: %~dpn1.csv
        set already_exists=1
        exit /b 0
    )
    echo %presentmon% %pmargs% -etl_file %1 -output_file "%~dpn1.csv"
    %presentmon% %pmargs% -etl_file %1 -output_file "%~dpn1.csv" >NUL
    exit /b 0
