--echo #
--echo # Reinstalls the plugin while keeping the rules table.
--echo #

CREATE TABLE t1 ( c1 VARCHAR(10), c2 VARCHAR(10) );
INSERT INTO t1 VALUES ( 'abc', 'def' ), ( 'ghi', 'klm' ), ( 'nop', 'qrs' );

CREATE TABLE t2 ( c1 VARCHAR(10) );
INSERT INTO t2 VALUES ( 'abc' ), ( 'klm' );

--source suite/query_rewrite_plugins/include/install_rewriter.inc

--echo # Insert some rewrite rules in the plugin
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SELECT * FROM test.t2',
         'SELECT * FROM test.t1 JOIN test.t2 ON test.t1.c1 = test.t2.c1' );

CALL query_rewrite.flush_rewrite_rules();

--echo # Check a query that doesn't have a rewrite rule doesn't get rewritten.
SELECT c2 FROM test.t1;

--echo # Check that we can execute a rewrite rule.
SELECT * FROM test.t2;

DROP PROCEDURE query_rewrite.flush_rewrite_rules;
DROP FUNCTION load_rewrite_rules;
UNINSTALL PLUGIN rewriter;
--source include/disconnect_connections.inc

--echo # Check that the table is still present and didn't get dropped.
SELECT pattern, replacement FROM query_rewrite.rewrite_rules;

--echo # Check that when the plugin is inactive the queries don't get rewritten.
SELECT * FROM test.t2;

--echo # Reinstall the plugin. All the rules should still be here from the
--echo # previous installation.
--source suite/query_rewrite_plugins/include/install_rewriter.inc

--echo # Check that a query that doesn't have a rewrite rule doesn't get
--echo # rewritten.
SELECT c2 FROM test.t1;

--echo # Check that we can still execute a rewrite rule.
SELECT * FROM test.t2;

DROP TABLE t1, t2;

--source suite/query_rewrite_plugins/include/uninstall_rewriter.inc
