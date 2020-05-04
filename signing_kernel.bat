@echo off
set Sign="%~dp0..\Common\Signing\signing_kernel.bat"

call %Sign% "%~dp0Bin/x64/SbieRelease/SbieDrv.sys"
call %Sign% "%~dp0Bin/Win32/SbieRelease/SbieDrv.sys"
