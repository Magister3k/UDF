SET TERM ^ ;
CREATE PROCEDURE check_id1_data
RETURNS (has_data INTEGER)
AS
DECLARE VARIABLE rec_in BLOB SUB_TYPE 0;
BEGIN
  SELECT rec FROM speech WHERE id = 1 INTO :rec_in;
  IF (rec_in IS NOT NULL) THEN
    has_data = 1;
  ELSE
    has_data = 0;
  SUSPEND;
END^
SET TERM ; ^
COMMIT;

EXECUTE PROCEDURE check_id1_data;
COMMIT;

DROP PROCEDURE check_id1_data;
COMMIT;