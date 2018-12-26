# Query rewrite plugin was installed.
#
# Bug#21619780: TABLE HINTS ARE LOST IN NORMALIZED PRINT OF STATEMENTS
# BEFORE RESOLVE
#
CREATE TABLE t1 (a INTEGER);
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ('SELECT /*+ NO_MRR(t1 idx1) */ * FROM test.t1',
'SELECT /*+ MRR(t1) */ * FROM test.t1');
CALL query_rewrite.flush_rewrite_rules();
SELECT normalized_pattern FROM query_rewrite.rewrite_rules;
normalized_pattern
select /*+ NO_MRR(`t1`@`select#1` `idx1`) */ `*` from `test`.`t1`
DELETE FROM query_rewrite.rewrite_rules;
DROP TABLE t1;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
# Query rewrite plugin was queued for uninstalling.
