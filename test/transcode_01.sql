SET TERM ^ ;
CREATE PROCEDURE transcode_0_to_1
AS
DECLARE VARIABLE rec_in BLOB SUB_TYPE 0;
DECLARE VARIABLE rec_out BLOB SUB_TYPE 0;
BEGIN
  /* Read from id=0 (has 13.g723) */
  SELECT rec FROM speech WHERE id = 0 INTO :rec_in;
  
  IF (rec_in IS NOT NULL) THEN
  BEGIN
    /* Transcode G.723.1 -> PCMU */
    SELECT rec_out FROM TRANSCODE_G723(:rec_in) INTO :rec_out;
    
    IF (rec_out IS NOT NULL) THEN
    BEGIN
      /* Write to id=1 */
      UPDATE speech SET rec = :rec_out, rectype = 'PCMU' WHERE id = 1;
    END
  END
END^
SET TERM ; ^
COMMIT;

EXECUTE PROCEDURE transcode_0_to_1;
COMMIT;

DROP PROCEDURE transcode_0_to_1;
COMMIT;