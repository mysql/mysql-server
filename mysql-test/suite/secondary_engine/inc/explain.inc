--disable_query_log
eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN';
--enable_query_log

--echo #
--echo # Bug#28556942: ADD USING SECONDARY ENGINE TO EXPLAIN
--echo #
CREATE TABLE t1 (a INT) SECONDARY_ENGINE MOCK;
--echo # Before table is loaded, the secondary engine should not be used.
ANALYZE TABLE t1;
# Traditional format is not supported with hypergraph.
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXPLAIN SELECT * FROM t1;
EXPLAIN FORMAT=JSON SELECT * FROM t1;
ALTER TABLE t1 SECONDARY_LOAD;
--echo # After table is loaded, the secondary engine should be used.
ANALYZE TABLE t1;
# As an exception, the traditional format is allowed with hypergraph
# when using a secondary engine. It does not show the full plan.
EXPLAIN SELECT * FROM t1;
EXPLAIN FORMAT=JSON SELECT * FROM t1, t1 t2 WHERE t1.a = t2.a;
DROP TABLE t1;

--disable_query_log
UNINSTALL PLUGIN mock;
--enable_query_log
