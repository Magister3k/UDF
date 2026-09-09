UPDATE speech 
SET rec = (
    SELECT rec_out 
    FROM TRANSCODE_G723(
        (SELECT rec FROM speech WHERE id = 0)
    )
)
WHERE id = 1;
