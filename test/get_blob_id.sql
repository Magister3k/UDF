SET LIST ON;

/* id = 0 */
OUTPUT 'd:\Projects\C++\UDF\test\blob_id_0.txt';

SELECT rec FROM speech WHERE id = 0;

OUTPUT;

/* id = 1 */
OUTPUT 'd:\Projects\C++\UDF\test\blob_id_1.txt';

SELECT rec FROM speech WHERE id = 1;

OUTPUT;

QUIT;
