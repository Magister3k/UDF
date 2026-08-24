/* 1. Удаляем оставшуюся временную процедуру (если она была) */
DROP PROCEDURE tmp_upd_speech;
COMMIT;

/* 2. Меняем разделитель на ^ */
SET TERM ^ ;

/* 3. Создаем временную процедуру */
CREATE PROCEDURE tmp_upd_speech
AS
DECLARE VARIABLE cur_id INTEGER;
DECLARE VARIABLE rec_in BLOB;
DECLARE VARIABLE rec_out BLOB;
BEGIN
    /* Цикл по всем записям */
    FOR SELECT id, REC FROM speech INTO :cur_id, :rec_in DO
    BEGIN
        /* Вызываем процедуру транскодирования через SELECT INTO, 
           так как transcode_g723 содержит оператор SUSPEND */
        SELECT rec_out FROM transcode_g723(:rec_in) INTO :rec_out;
        
        /* В PSQL двоеточие перед rec_out в условии IF не требуется */
        IF (rec_out IS NOT NULL) THEN
        BEGIN
            /* Обновляем поля для текущей строки курсора */
            UPDATE speech
               SET REC = :rec_out,
                   rectype = 'PCMU'
             WHERE id = :cur_id;
        END
    END /* <- Обязательный END для закрытия цикла FOR SELECT */
END^

/* 4. Возвращаем стандартный разделитель */
SET TERM ; ^
COMMIT;

/* 5. Запускаем только что созданную процедуру */
EXECUTE PROCEDURE tmp_upd_speech;
COMMIT;

/* 6. Удаляем временную процедуру после успешного выполнения */
DROP PROCEDURE tmp_upd_speech;
COMMIT;
