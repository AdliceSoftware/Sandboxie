@echo off
set Sign="%~dp0..\Common\Signing\signing_user.bat"

call %Sign% "%~dp0Bin/Win32/SandboxieInstall32.exe"
REM ==================================================