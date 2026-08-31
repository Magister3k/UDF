@echo off
setlocal enabledelayedexpansion

echo Testing 5.3kbps decoder test vectors...

for %%f in (tv\*.c5h) do (
    set "fname=%%~nf"
    echo Testing !fname!...
    .\lbccodec_ref.exe -d -r53 "tv\!fname!.c5h" "ref_out_!fname!.r5p" >nul 2>&1
    .\checksnr.exe "tv\!fname!.r5p" "ref_out_!fname!.r5p" -snrmin63 2>&1 | findstr "PASSED FAILED"
)

echo.
echo Testing 6.3kbps decoder test vectors...
for %%f in (tv\*.c6h) do (
    set "fname=%%~nf"
    echo Testing !fname!...
    .\lbccodec_ref.exe -d -r63 "tv\!fname!.c6h" "ref_out_!fname!.r6p" >nul 2>&1
    .\checksnr.exe "tv\!fname!.r6p" "ref_out_!fname!.r6p" -snrmin63 2>&1 | findstr "PASSED FAILED"
)

echo.
echo Testing encoder 5.3kbps...
.\lbccodec_ref.exe -c -r53 -R10 tv\tst53c.bin test_enc.c53 >nul 2>&1
.\cmpcode.exe -R10 tv\tst53c.c53 test_enc.c53

echo.
echo Testing encoder 6.3kbps...
.\lbccodec_ref.exe -c -r63 -R10 tv\tst63c.bin test_enc.c63 >nul 2>&1
.\cmpcode.exe -R10 tv\tst63c.c63 test_enc.c63

echo.
echo All tests complete.