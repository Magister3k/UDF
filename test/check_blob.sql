SET TERM ^ ;
SELECT 
  (SELECT cast(octet_length(rec) as integer) FROM speech WHERE id = 0) as src_size,
  (SELECT cast(octet_length(rec) as integer) FROM speech WHERE id = 1) as dst_size
FROM rdb$database^
COMMIT^
SET TERM ; ^