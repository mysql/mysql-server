--source include/have_debug.inc

# Does not contain the sync points; most likely, the test is just irrelevant
# for the hypergraph optimizer.
--source include/not_hypergraph.inc

SET @old_debug= @@session.debug;

--echo Bug#13820776 CRASH IN HANDLER::HA_STATISTIC_INCREMENT IF QUERY IS KILLED

CREATE TABLE g9(a INT) ENGINE=INNODB;

let $query=SELECT 1 FROM g9
RIGHT JOIN (SELECT 1 FROM g9) AS d1 ON 1 LEFT JOIN
(SELECT 1 FROM g9) AS d2 ON 1;

eval $query;

SET debug= '+d,bug13820776_1';
--error ER_QUERY_INTERRUPTED
eval $query;
SET debug= '-d,bug13820776_1';

SET debug= '+d,bug13820776_2';
--error ER_QUERY_INTERRUPTED
eval $query;
SET debug= '-d,bug13820776_2';

DROP TABLE g9;
SET debug= @old_debug;

--echo Bug#13822652 TESTCASES: PROTOCOL.CC:518: VOID PROTOCOL::END_STATEMENT(): ASSERTION `0' FAILED

CREATE TABLE g1(a INT PRIMARY KEY, b INT) ENGINE=INNODB;
INSERT INTO g1 VALUES (1,2),(2,3),(4,5);
CREATE TABLE g2(a INT PRIMARY KEY, b INT) ENGINE=INNODB;
INSERT INTO g2 VALUES (1,2),(2,3),(4,5);

let $query=UPDATE IGNORE g1,g2 SET g1.b=0 WHERE g1.a=g2.a;
eval $query;

SET debug= '+d,bug13820776_2';

--error ER_QUERY_INTERRUPTED
eval $query;
SET debug= '-d,bug13820776_2';

SET debug= '+d,bug13822652_1';
--error ER_QUERY_INTERRUPTED
eval $query;
SET debug= '-d,bug13822652_1';

let $query=INSERT IGNORE INTO g1(a) SELECT b FROM g1 WHERE a<=0 LIMIT 5;
eval $query;

SET debug= '+d,bug13822652_2';
--error ER_QUERY_INTERRUPTED
eval $query;
SET debug= '-d,bug13822652_2';

DROP TABLE g1,g2;

--echo #
--echo # Bug#28079850: Requires handling THD::KILL_QUERY in records* functions.
--echo #

SET GLOBAL innodb_limit_optimistic_insert_debug = 2;

CREATE TABLE t_innodb(c1 INT NOT NULL PRIMARY KEY,
                      c2 INT NOT NULL,
                      c3 char(20),
                      KEY c3_idx(c3))ENGINE=INNODB;

INSERT INTO t_innodb VALUES (1, 1, 'a'), (2,2,'a'), (3,3,'a');
ANALYZE TABLE t_innodb;

SET debug= '+d,bug28079850';
#checks handler::records_from_index()
--error ER_QUERY_INTERRUPTED
SELECT COUNT(*) FROM t_innodb;
#checks handler::records()
--error ER_QUERY_INTERRUPTED
SELECT COUNT(*) FROM performance_schema.table_handles;
SET debug= '-d,bug28079850';

DROP TABLE t_innodb;

SET debug= @old_debug;

--echo Bug#29969769 - KILL QUERY KILLS THE NEXT QUERY
# Sending KILL QUERY before statement begins execution shall not interrupt it.
connect (con1, localhost, root,,);
--let $ID= `SELECT connection_id()`
SET DEBUG_SYNC='before_command_dispatch SIGNAL kill_query WAIT_FOR continue';
--send SELECT 1;

connection default;
SET DEBUG_SYNC='now WAIT_FOR kill_query';
--replace_result $ID ID
--eval KILL QUERY $ID
SET DEBUG_SYNC='now SIGNAL continue';

connection con1;
#Without fix, SELECT fails with ER_QUERY_INTERRUPTED error.
--reap

connection default;
SET DEBUG_SYNC='RESET';
disconnect con1;

SET GLOBAL innodb_limit_optimistic_insert_debug = 0;
