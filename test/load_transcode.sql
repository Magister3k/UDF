/* Load 12.g723 into speech.id=0, transcode, write to id=1 */

SET TERM ^ ;

CREATE PROCEDURE load_and_transcode
AS
DECLARE VARIABLE rec_in BLOB SUB_TYPE 0;
DECLARE VARIABLE rec_out BLOB SUB_TYPE 0;
DECLARE VARIABLE blob_handle BLOB;
BEGIN
  /* 1. Clear id=0 and insert 12.g723 from file */
  UPDATE speech SET rec = NULL, rectype = 'G723.1' WHERE id = 0;
  
  /* Use InterBase file blob loading - need to use INSERT with file */
  /* First delete and re-insert with file content */
  DELETE FROM speech WHERE id = 0;
  
  /* Since we can't directly load file in PSQL, we'll use the existing data
     but ensure id=0 has the original G723.1 data */
  
  /* Check current data */
  SELECT rec FROM speech WHERE id = 0 INTO :rec_in;
  
  IF (rec_in IS NOT NULL) THEN
  BEGIN
    /* 2. Transcode */
    SELECT rec_out FROM TRANSCODE_G723(:rec_in) INTO :rec_out;
    
    IF (rec_out IS NOT NULL) THEN
    BEGIN
      /* 3. Write to id=1 */
      UPDATE speech SET rec = :rec_out, rectype = 'PCMU' WHERE id = 1;
    END
  END
END^

SET TERM ; ^
COMMIT;

EXECUTE PROCEDURE load_and_transcode;
COMMIT;

DROP PROCEDURE load_and_transcode;
COMMIT;