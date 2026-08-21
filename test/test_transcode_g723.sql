/* 1. Меняем разделитель на ^ */
SET TERM ^ ;

/* 2. Создаем временную процедуру */
CREATE PROCEDURE tmp_update_speech
AS
DECLARE VARIABLE current_id INTEGER;
DECLARE VARIABLE old_rec BLOB;
DECLARE VARIABLE new_rec BLOB;
BEGIN
    /* Цикл по всем записям */
    FOR SELECT id, rec FROM speech INTO :current_id, :old_rec DO
    BEGIN
        /* Вызываем вашу процедуру транскодирования */
        EXECUTE PROCEDURE transcode_g723(:old_rec) RETURNING_VALUES (:new_rec);
	IF (:new_rec IS NOT NULL) THEN
        BEGIN
	        /* Обновляем поля (исправлен синтаксис SET через запятую) */
	        UPDATE speech
	        SET rec = :new_rec,
	            rectype = 'PCMU'
	        WHERE id = :current_id;
        END
    END
END^

/* 3. Возвращаем стандартный разделитель */
SET TERM ; ^
COMMIT;

/* 4. Запускаем только что созданную процедуру */
EXECUTE PROCEDURE tmp_update_speech;
COMMIT;

/* 5. Удаляем процедуру, чтобы не засырять базу данных */
DROP PROCEDURE tmp_update_speech;
COMMIT;
