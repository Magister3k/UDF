/* 1. Удаляем оставшуюся временную процедуру (если она была) */
DROP PROCEDURE tmp_upd_speech;
COMMIT;

/* 2. Создаем ГЛОБАЛЬНУЮ ВРЕМЕННУЮ ТАБЛИЦУ (она изолирована для вашей сессии) */
/* Если она уже создана, InterBase пропустит этот шаг, но перед этим мы её очистим */
CREATE GLOBAL TEMPORARY TABLE tmp_speech_ids (
    id_to_upd INTEGER
) ON COMMIT PRESERVE ROWS;
COMMIT;

SET TERM ^ ;

/* 3. Создаем временную процедуру */
CREATE PROCEDURE tmp_upd_speech
AS
DECLARE VARIABLE cur_id INTEGER;
DECLARE VARIABLE rec_in BLOB;
DECLARE VARIABLE rec_out BLOB;
BEGIN
    /* Очищаем временную таблицу на случай, если там что-то было */
    DELETE FROM tmp_speech_ids;

    /* ШАГ 1: Быстро собираем ID всех строк, которые нужно обработать. */
    /* Это моментальная операция чтения, она не накладывает долгосрочных блокировок. */
    INSERT INTO tmp_speech_ids (id_to_upd)
    SELECT id FROM speech;

    /* ШАГ 2: Теперь запускаем цикл по временной таблице! */
    /* Курсор висит на таблице tmp_speech_ids, поэтому мы можем */
    /* безболезненно и без дедлоков делать UPDATE оригинальной таблицы speech. */
    FOR SELECT id_to_upd FROM tmp_speech_ids INTO :cur_id DO
    BEGIN
        /* Изолированно читаем BLOB конкретной строки */
        SELECT rec FROM speech WHERE id = :cur_id INTO :rec_in;

        IF (rec_in IS NOT NULL) THEN
        BEGIN
            /* Вызываем транскодер */
            SELECT rec_out FROM transcode_g723(:rec_in) INTO :rec_out;
            
            IF (rec_out IS NOT NULL) THEN
            BEGIN
                UPDATE speech
                   SET rec = :rec_out,
                       rectype = 'PCMU'
                 WHERE id = :cur_id;
            END
        END
    END
    
    /* Очищаем временную таблицу */
    DELETE FROM tmp_speech_ids;
END^

SET TERM ; ^
COMMIT;

/* 4. Запускаем выполнение созданной процедуры */
EXECUTE PROCEDURE tmp_upd_speech;
COMMIT;

/* 5. Удаляем временную процедуру */
DROP PROCEDURE tmp_upd_speech;
COMMIT;
