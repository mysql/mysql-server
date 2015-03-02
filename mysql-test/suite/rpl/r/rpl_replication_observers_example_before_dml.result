include/have_replication_observers_example_plugin.inc
include/install_replication_observers_example.inc
#
# Verify that the DML hook is in place and working
#
CREATE TABLE t2 (c1 char(50) NOT NULL PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE tn (cn char(50) NOT NULL PRIMARY KEY) ENGINE=InnoDB;
SET @debug_saved= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,cause_failure_in_before_dml_hook';
INSERT INTO t2 VALUES('a');
ERROR HY000: The table does not comply with the requirements by an external plugin.
UPDATE t2 SET c1 = 'a';
ERROR HY000: The table does not comply with the requirements by an external plugin.
INSERT INTO t2 (c1) SELECT tn.cn FROM tn;
ERROR HY000: The table does not comply with the requirements by an external plugin.
DELETE FROM t2;
ERROR HY000: The table does not comply with the requirements by an external plugin.
LOAD DATA INFILE '../../std_data/words.dat' INTO TABLE t2;
ERROR HY000: The table does not comply with the requirements by an external plugin.
REPLACE INTO t2 VALUES('a');
ERROR HY000: The table does not comply with the requirements by an external plugin.
REPLACE INTO t2 (c1) SELECT tn.cn FROM tn;
ERROR HY000: The table does not comply with the requirements by an external plugin.
DELETE t2, tn FROM t2, tn;
ERROR HY000: The table does not comply with the requirements by an external plugin.
UPDATE t2, tn SET c1 = 'a';
ERROR HY000: The table does not comply with the requirements by an external plugin.
#
# Cleanup
#
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t2;
DROP TABLE tn;
include/uninstall_replication_observers_example.inc
End of test
