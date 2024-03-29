--skip_if_hypergraph  # Depends on the query plan.
--eval EXPLAIN FORMAT=JSON $sql_to_do

BEGIN;
--eval $sql_to_do
ROLLBACK;

BEGIN;
--eval $sql_to_do
COMMIT;
