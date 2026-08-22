$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
mingw32-make.exe clean
mingw32-make.exe build
mingw32-make.exe test
pause