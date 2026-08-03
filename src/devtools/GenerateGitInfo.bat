@echo off
setlocal EnableDelayedExpansion

::----------------------------------------------------------
:: Git information
::----------------------------------------------------------

for /f %%i in ('git rev-parse HEAD') do set GIT_SHA=%%i
for /f %%i in ('git rev-parse --short HEAD') do set GIT_SHA_SHORT=%%i
for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set GIT_BRANCH=%%i
for /f "delims=" %%i in ('git describe --tags --always') do set GIT_VERSION=%%i
for /f %%i in ('git rev-list --count HEAD') do set GIT_REVISION=%%i

::----------------------------------------------------------
:: Dirty flag
::----------------------------------------------------------

git diff --quiet
if errorlevel 1 (
    set GIT_DIRTY=1
) else (
    set GIT_DIRTY=0
)

::----------------------------------------------------------
:: Count modified / untracked files
::----------------------------------------------------------

set MODIFIED=0
set UNTRACKED=0
set CHANGED_FILES=

for /f "delims=" %%i in ('git status --porcelain') do (

    set LINE=%%i

    if "!LINE:~0,2!"=="??" (
        set /a UNTRACKED+=1
    ) else (
        set /a MODIFIED+=1
    )

    set CHANGED_FILES=!CHANGED_FILES!%%i\n
)

::----------------------------------------------------------
:: Generate header
::----------------------------------------------------------

(
echo #pragma once
echo.
echo #define GIT_COMMIT        "%GIT_SHA%"
echo #define GIT_COMMIT_SHORT  "%GIT_SHA_SHORT%"
echo #define GIT_BRANCH        "%GIT_BRANCH%"
echo #define GIT_VERSION       "%GIT_VERSION%"
echo #define GIT_REVISION      "%GIT_REVISION%"
echo.
echo #define GIT_DIRTY         %GIT_DIRTY%
echo #define GIT_MODIFIED      %MODIFIED%
echo #define GIT_UNTRACKED     %UNTRACKED%
echo.
echo #define GIT_CHANGED_FILES "%CHANGED_FILES%"
echo.
) > "%cd%\git_info.h"

endlocal