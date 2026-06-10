@echo off
REM Build + install TakeMeIn EQ VST3. Run from anywhere.
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "SRC=c:\Users\SysAdmin\Desktop\Completed Projects\TakeMeIn"
set "OUT=%SRC%\build\VST3\Release\TakeMeInEQ.vst3"
set "DEST=C:\Program Files\Common Files\VST3\TakeMeInEQ.vst3"

call "%VCVARS%" >nul 2>&1
"%CMAKE%" --build "%SRC%\build" --target TakeMeInEQ
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo Installing to %DEST% ...
robocopy "%OUT%" "%DEST%" /MIR /NJH /NJS /NDL /NFL >nul
echo DONE
exit /b 0
