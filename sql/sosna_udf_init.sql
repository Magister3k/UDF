/*
   Execute this script while connected to the target InterBase database (db).
   Copy sosna_udf.dll to the server's InterBase\UDF directory first.
*/

DECLARE EXTERNAL FUNCTION TRANSCODE_G723
    BLOB,
    BLOB
    RETURNS PARAMETER 2
    ENTRY_POINT 'transcode_g723'
    MODULE_NAME 'sosna_udf';

SET TERM ^ ;

CREATE PROCEDURE TRANSCODE_SPEECH_REC (
    P_REC BLOB SUB_TYPE 0
)
RETURNS (
    CONVERTED_REC BLOB SUB_TYPE 0
)
AS
BEGIN
    IF (P_REC IS NOT NULL) THEN
        CONVERTED_REC = TRANSCODE_G723(P_REC);
    ELSE
        CONVERTED_REC = NULL;
    SUSPEND;
END^

SET TERM ; ^

/*
   Example for converting the speech.rec column in database db:

   UPDATE speech
      SET rec = (SELECT converted_rec
                   FROM TRANSCODE_SPEECH_REC(speech.rec))
    WHERE rec IS NOT NULL;
*/