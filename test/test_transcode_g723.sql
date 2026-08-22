/* 1. Меняем разделитель на ^ */
SET TERM ^ ;

/* 2. Создаем временную процедуру */
CREATE PROCEDURE tmp_upd_speech
AS
DECLARE VARIABLE cur_id INTEGER;
DECLARE VARIABLE rec_in BLOB;
DECLARE VARIABLE rec_out BLOB;
BEGIN
    /* Цикл по всем записям */
    FOR SELECT id, rec FROM speech INTO :cur_id, :rec_in DO
    BEGIN
        /* Вызываем процедуру транскодирования */
        EXECUTE PROCEDURE transcode_g723(:rec_in) RETURNING_VALUES (:rec_out);
	IF (:rec_out IS NOT NULL) THEN
        BEGIN
	        /* Обновляем поля */
	        UPDATE speech
	        SET rec = :rec_out,
	            rectype = 'PCMU'
	        WHERE id = :cur_id;
        END
    END
END^

/* 3. Возвращаем стандартный разделитель */
SET TERM ; ^
COMMIT;

/* 4. Запускаем только что созданную процедуру */
EXECUTE PROCEDURE tmp_upd_speech;
COMMIT;

/* 5. Удаляем процедуру, чтобы не засорять базу данных */
DROP PROCEDURE tmp_upd_speech;
COMMIT;
