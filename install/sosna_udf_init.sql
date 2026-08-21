/*
   Execute this script while connected to the target InterBase database (db).
   Copy sosna_udf.dll to the server's InterBase\UDF directory first.
*/

DECLARE EXTERNAL FUNCTION UDF_TRANSCODE_G723
    BLOB,
    BLOB
    RETURNS PARAMETER 2
    ENTRY_POINT 'transcode_g723'
    MODULE_NAME 'sosna_udf';

COMMIT;

SET TERM ^ ;

CREATE PROCEDURE TRANSCODE_G723 (
    REC_IN BLOB SUB_TYPE 0
)
RETURNS (
    REC_OUT BLOB SUB_TYPE 0
)
AS
BEGIN
    IF (REC_IN IS NOT NULL) THEN
        REC_OUT = UDF_TRANSCODE_G723(REC_IN);
    ELSE
        REC_OUT = NULL;
    SUSPEND;
END^

SET TERM ; ^

/*
   Example for converting the speech.rec column in database:

   UPDATE speech
      SET rec = (SELECT converted_rec
                   FROM TRANSCODE_G723(speech.rec))
    WHERE rec IS NOT NULL;
*/