@echo off
echo Compiling Indra APC Plugin...

set SOURCES=src\Plugin.cpp src\Screen.cpp src\Storage.cpp
set OUTPUT=build\IndraApc.dll
set ES_LIB=

if exist "EuroScopePlugIn.lib"        set ES_LIB=EuroScopePlugIn.lib
if exist "EuroScopePlugInDll.lib"     set ES_LIB=EuroScopePlugInDll.lib
if exist "lib\EuroScopePlugIn.lib"    set ES_LIB=lib\EuroScopePlugIn.lib
if exist "lib\EuroScopePlugInDll.lib" set ES_LIB=lib\EuroScopePlugInDll.lib

if "%ES_LIB%"=="" (
    echo Missing EuroScope SDK import library.
    echo Copy EuroScopePlugIn.lib or EuroScopePlugInDll.lib into this folder,
    echo or put it in a lib subfolder, then run compile.bat again.
    exit /b 2
)

echo Using EuroScope library: %ES_LIB%

if not exist build mkdir build

cl /nologo /LD /MD /O2 /Oi /GL /EHsc ^
    /std:c++14 ^
    /D "_CRT_SECURE_NO_WARNINGS" ^
    /D "WIN32_LEAN_AND_MEAN" ^
    /I "include" ^
    /I "src" ^
    /Fe"%OUTPUT%" ^
    %SOURCES% ^
    /link /SUBSYSTEM:WINDOWS /MACHINE:X86 /LTCG /OPT:REF /OPT:ICF /DLL ^
    "%ES_LIB%" user32.lib gdi32.lib

if %errorlevel% == 0 (
    echo Compilation successful! Output: %OUTPUT%
) else (
    echo Compilation failed with error code %errorlevel%
)