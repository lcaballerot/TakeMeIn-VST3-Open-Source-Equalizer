@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >/dev/null 2>&1
set PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d "c:\Users\SysAdmin\Desktop\Completed Projects\TakeMeIn"
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_VST3SDK="c:/Users/SysAdmin/Desktop/Completed Projects/TakeMeIn/build/_deps/vst3sdk-src"
