$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
mingw32-make.exe info
pause

mingw32-make.exe clean
pause
mingw32-make.exe build
pause
mingw32-make.exe test
pause