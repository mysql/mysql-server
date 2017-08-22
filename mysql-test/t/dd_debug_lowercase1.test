#
# Tests for data-dictionary implementation requiring debug build of server
# and lower_case_table_names = 1
#

--source include/have_debug.inc

--echo #
--echo # Bug#25495714: FOREIGN KEY INFORMATION IN NEW DD NOT FOLLOW
--echo #               THE LOWER CASE TABLE NAME SETTING
--echo #

CREATE TABLE t1 (c1 INT PRIMARY KEY);
CREATE TABLE t2 (c1 INT, FOREIGN KEY (c1) REFERENCES TEST.T1 (c1));

SHOW CREATE TABLE t2;
SELECT unique_constraint_schema, referenced_table_name
  FROM information_schema.referential_constraints WHERE table_name = 't2';
SET SESSION debug= '+d,skip_dd_table_access_check';
SELECT referenced_table_schema, referenced_table_name
   FROM mysql.foreign_keys, mysql.tables
  WHERE tables.name= 't2' AND foreign_keys.table_id = tables.id;
SET SESSION debug= '-d,skip_dd_table_access_check';

DROP TABLE t2, t1;
