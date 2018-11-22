FLUSH BINARY LOGS;
#
# TC1. Check `CREATE TABLE` has `Q_SQL_REQUIRE_PRIMARY_KEY` flag set
# ------------------------------------------------------------------
SET @@session.sql_require_primary_key=0;
CREATE TABLE tnopk (col1 INT, col2 INT);
include/assert_grep.inc ['CREATE TABLE' with no PK has 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set and with value '0']
SET @@session.sql_require_primary_key=1;
CREATE TABLE tpk (col1 INT PRIMARY KEY, col2 INT);
include/assert_grep.inc ['CREATE TABLE' with PK has 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set and with value '1']
#
# TC2. Check DML `INSERT`, `UPDATE` and `DELETE` do not have the flag set
# -----------------------------------------------------------------------
INSERT INTO tnopk VALUES (1, 0), (2, 0); INSERT INTO tpk VALUES (1, 0), (2, 0);
include/assert_grep.inc ['INSERT' statements do not have 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set]
UPDATE tnopk SET col2 = 1 WHERE col1 = 1; UPDATE tpk SET col2 = 1 WHERE col1 = 1;
include/assert_grep.inc ['UPDATE' statements do not have 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set]
DELETE FROM tnopk where col1 = 2; DELETE FROM tpk where col1 = 2;
include/assert_grep.inc ['DELETE' statements do not have 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set]
#
# TC3. Check `ALTER TABLE` has `Q_SQL_REQUIRE_PRIMARY_KEY` flag set
# ------------------------------------------------------------------
SET @@session.sql_require_primary_key=0;
ALTER TABLE tnopk ADD COLUMN (col3 INT);
include/assert_grep.inc ['ALTER TABLE' in table with no PK has 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set and with value '0']
SET @@session.sql_require_primary_key=1;
ALTER TABLE tpk ADD COLUMN (col3 INT);
include/assert_grep.inc ['ALTER TABLE' in table with PK has 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set and with value '1']
#
# TC4. Check `DROP TABLE` do not has `Q_SQL_REQUIRE_PRIMARY_KEY` flag set
# -----------------------------------------------------------------------
SET @@session.sql_require_primary_key=0;
DROP TABLE tnopk;
include/assert_grep.inc ['DROP TABLE' statements, of table with no PK, do not have 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set]
SET @@session.sql_require_primary_key=1;
DROP TABLE tpk;
include/assert_grep.inc ['DROP TABLE' statements, of table with PK, do not have 'Q_SQL_REQUIRE_PRIMARY_KEY' flag set]
