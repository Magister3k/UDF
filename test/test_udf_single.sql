UPDATE speech 
SET rec = UDF_TRANSCODE_G723((SELECT rec FROM speech WHERE id = 0))
WHERE id = 1;