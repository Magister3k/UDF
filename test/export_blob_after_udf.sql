/* Переводим ISQL в режим принудительного вывода бинарных данных */
SET BLOBDISPLAY ALL;

/* Направляем поток вывода во внешний файл */
OUTPUT 'd:\Projects\C++\UDF\test\rec_after_udf.bin';

/* Вызываем вашу рабочую функцию */
SELECT UDF_TRANSCODE_G723(rec) FROM speech WHERE id = 0;

OUTPUT;
QUIT;
