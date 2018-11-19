# TC.1 `BINLOG` + `Format_description` event inside an active transaction
# -----------------------------------------------------------------------
BEGIN;
BINLOG
'3cTpWw8BAAAAeAAAAHwAAAABAAQAOC4wLjE0LWRlYnVnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAADdxOlbEwANAAgAAAAABAAEAAAAYAAEGggAAAAICAgCAAAACgoKKioAEjQA
CgEyBOzV';
ERROR HY000: The system variable gtid_next cannot be set when there is an ongoing transaction.
COMMIT;
# TC.2 `BINLOG` + `Format_description` event with no active transaction
# ---------------------------------------------------------------------
BINLOG
'3cTpWw8BAAAAeAAAAHwAAAABAAQAOC4wLjE0LWRlYnVnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAADdxOlbEwANAAgAAAAABAAEAAAAYAAEGggAAAAICAgCAAAACgoKKioAEjQA
CgEyBOzV';
include/assert.inc [Value of `GTID_NEXT` is `NOT_YET_DETERMINED`]
# TC.3 `GTID_NEXT` = 'NOT_YET_DETERMINED' and try to execute a statement
# ----------------------------------------------------------------------
CREATE TABLE t1 (col1 INT);
include/assert.inc [Value of `GTID_NEXT` is `ANONYMOUS`]
# Clean up.
SET @@GTID_NEXT = "AUTOMATIC";
DROP TABLE t1;
