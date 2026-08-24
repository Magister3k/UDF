/*
   Подключение функции transcode_g723 из библиотеки sosna_udf.dll
   Выполните этот сценарий при подключении к целевой базе данных InterBase
   Предварительно скопируйте файл sosna_udf.dll в каталог InterBase\UDF на сервере
*/

/* 1. Удаляем старую регистрацию */
/* Используем DROP, игнорируя ошибки, если объектов еще не существует */
DROP PROCEDURE TRANSCODE_G723;
DROP EXTERNAL FUNCTION UDF_TRANSCODE_G723;
COMMIT;

/* 2. Регистрируем внешнюю функцию */
DECLARE EXTERNAL FUNCTION UDF_TRANSCODE_G723
    BLOB,
    BLOB
    RETURNS PARAMETER 2
    ENTRY_POINT 'transcode_g723'
    MODULE_NAME 'sosna_udf';
COMMIT;

/* 3. Меняем терминатор для создания процедуры */
SET TERM ^ ;

/* 4. Создаем хранимую процедуру-обертку */
CREATE PROCEDURE TRANSCODE_G723 (
    REC_IN BLOB SUB_TYPE 0
)
RETURNS (
    REC_OUT BLOB SUB_TYPE 0
)
AS
BEGIN
    IF (REC_IN IS NOT NULL) THEN
    BEGIN
        REC_OUT = UDF_TRANSCODE_G723(REC_IN);
    END
    ELSE
    BEGIN
        REC_OUT = NULL;
    END
    SUSPEND;
END^

/* 5. Возвращаем стандартный терминатор */
SET TERM ; ^
COMMIT;

/*
   Пример использования для обновления таблицы speech:

   UPDATE speech
      SET rec = (SELECT REC_OUT FROM TRANSCODE_G723(speech.rec))
    WHERE rec IS NOT NULL;
*/
