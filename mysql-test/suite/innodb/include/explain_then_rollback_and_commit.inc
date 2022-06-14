--eval EXPLAIN FORMAT=JSON $sql_to_do

BEGIN;
--eval $sql_to_do
ROLLBACK;

BEGIN;
--eval $sql_to_do
COMMIT;
