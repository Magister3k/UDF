@echo off

C:\InterBase\bin\isql.exe -user SYSDBA -password masterkey "localhost:d:\Projects\C++\UDF\test\db.gdb" -input "d:\Projects\C++\UDF\test\get_blob_id.sql"

for /f "tokens=2 delims= " %%A in ('findstr /c:":" d:\Projects\C++\UDF\test\blob_id_0.txt ^| findstr /v /i "display"') do (
    set BLOB_ID=%%A
)

echo BLOBDUMP %BLOB_ID% d:\Projects\C++\UDF\test\rec_id_0.bin; > d:\Projects\C++\UDF\test\export_blob_id_0.sql
echo QUIT; >> d:\Projects\C++\UDF\test\export_blob_id_0.sql

C:\InterBase\bin\isql.exe -user SYSDBA -password masterkey "localhost:d:\Projects\C++\UDF\test\db.gdb" -input "d:\Projects\C++\UDF\test\export_blob_id_0.sql"

for /f "tokens=2 delims= " %%A in ('findstr /c:":" d:\Projects\C++\UDF\test\blob_id_1.txt ^| findstr /v /i "display"') do (
    set BLOB_ID=%%A
)

echo BLOBDUMP %BLOB_ID% d:\Projects\C++\UDF\test\rec_id_1.bin; > d:\Projects\C++\UDF\test\export_blob_id_1.sql
echo QUIT; >> d:\Projects\C++\UDF\test\export_blob_id_1.sql

C:\InterBase\bin\isql.exe -user SYSDBA -password masterkey "localhost:d:\Projects\C++\UDF\test\db.gdb" -input "d:\Projects\C++\UDF\test\export_blob_id_1.sql"
