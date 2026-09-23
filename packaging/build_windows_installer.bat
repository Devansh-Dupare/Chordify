@echo off
rem Builds a Release of Chordify (VST3 + Standalone) and packages it with Inno Setup as
rem packaging\Output\Chordify-<version>-Windows.exe.
rem
rem Needs: Visual Studio 2022 (Desktop C++), CMake, Inno Setup 6 (iscc on PATH).
rem Signing (optional): set SIGN_COMMAND to a full signtool command line ending in the file to
rem sign as $f, for example with Azure Trusted Signing or an Authenticode certificate:
rem   set SIGN_COMMAND=signtool sign /fd sha256 /tr http://timestamp.digicert.com /td sha256 /a $f
rem Without it, the installer and plug-in are unsigned and SmartScreen will warn on other PCs.
rem
rem Usage: packaging\build_windows_installer.bat

setlocal
cd /d "%~dp0\.."

set BUILD_DIR=build-installer

echo ==^> Building Chordify (Release)
cmake -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build %BUILD_DIR% --config Release --target Chordify_VST3 Chordify_Standalone || exit /b 1

set ARTEFACTS=%BUILD_DIR%\Chordify_artefacts\Release

if defined SIGN_COMMAND (
    echo ==^> Signing the plug-in and app
    call %SIGN_COMMAND:$f="%ARTEFACTS%\VST3\Chordify.vst3\Contents\x86_64-win\Chordify.vst3"% || exit /b 1
    call %SIGN_COMMAND:$f="%ARTEFACTS%\Standalone\Chordify.exe"% || exit /b 1
    echo ==^> Building the signed installer
    iscc /DSign /DBuildDir="..\%BUILD_DIR%" "/Ssigntool=%SIGN_COMMAND%" packaging\installer.iss || exit /b 1
) else (
    echo ==^> SIGN_COMMAND not set: building an unsigned installer
    iscc /DBuildDir="..\%BUILD_DIR%" packaging\installer.iss || exit /b 1
)

echo ==^> Done: packaging\Output
endlocal
