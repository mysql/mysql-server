#
# Bug#30311271 RAND() < RAND() RESULTS IN IMPOSSIBLE WHERE
#
# See that the optimizer does not consider two instances of RAND() as
# equal.
EXPLAIN FORMAT=tree SELECT 1 WHERE RAND() < RAND();
EXPLAIN
-> Filter: (rand() < rand())  (cost=0.00 rows=1)
    -> Rows fetched before execution  (cost=0.00 rows=1)

