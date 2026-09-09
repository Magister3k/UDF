@echo off
call "c:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
cd D:\Projects\C++\UDF
cmake --build build_msvc --config Release