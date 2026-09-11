@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
cd D:\Projects\C++\UDF
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -B build_msvc .
if errorlevel 1 exit /b 1
cmake --build build_msvc --config Release
pause