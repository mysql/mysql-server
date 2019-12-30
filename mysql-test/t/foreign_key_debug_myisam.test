# Tests of foreign keys that need a debug build or debug_sync feature.
#

--source include/have_debug.inc
--source include/have_debug_sync.inc

# Some parts of the test require enabled binary log.
--source include/have_log_bin.inc

--source include/have_myisam.inc

SET SESSION debug= '+d,skip_dd_table_access_check';

--echo #
--echo # RENAME will update FK information in both children and parents, also
--echo # when there are tables in the statement that belong in SEs without
--echo # support for atomic DDL. Basically the same test case as above with
--echo # an additional MyISAM table.
--echo #

CREATE TABLE parent(pk INTEGER PRIMARY KEY, j INTEGER,
  UNIQUE KEY parent_key(j));

CREATE TABLE child(pk INTEGER PRIMARY KEY, k INTEGER, fk INTEGER,
  FOREIGN KEY (fk) REFERENCES parent(j), UNIQUE KEY child_key(k));

CREATE TABLE grandchild(pk INTEGER PRIMARY KEY, fk INTEGER,
  FOREIGN KEY (fk) REFERENCES child(k));

SET @@foreign_key_checks= 0;
CREATE TABLE orphan_grandchild(pk INTEGER PRIMARY KEY, fk INTEGER,
  FOREIGN KEY (fk) REFERENCES siebling(k));
SET @@foreign_key_checks= 1;

CREATE TABLE non_atomic_t1(pk INTEGER) ENGINE= MyISAM;

--echo # FK definitions before rename:
SELECT name, unique_constraint_name, referenced_table_schema, referenced_table_name
  FROM mysql.foreign_keys
  WHERE referenced_table_schema LIKE 'test';

RENAME TABLE non_atomic_t1 TO non_atomic_t2, child TO siebling;

--echo # After the rename, we see that:
--echo # 1. The name of the constraint is changed to 'siebling_ibfk...'.
--echo # 2. The referenced table name of the grandchild is changed to 'siebling'.
--echo # 3. The unique constraint name of the orphan_grandchild is corrected.
SELECT name, unique_constraint_name, referenced_table_schema, referenced_table_name
  FROM mysql.foreign_keys
  WHERE referenced_table_schema LIKE 'test';

DROP TABLE grandchild;
DROP TABLE orphan_grandchild;
DROP TABLE siebling;
DROP TABLE parent;
DROP TABLE non_atomic_t2;

--echo #
--echo # RENAME will rollback FK information in both children and parents, also
--echo # when there are tables in the statement that belong in SEs without
--echo # support for atomic DDL. Basically the same test case as above with
--echo # an additional MyISAM table, where renaming fails due to an existing table.
--echo #

CREATE TABLE parent(pk INTEGER PRIMARY KEY, j INTEGER,
  UNIQUE KEY parent_key(j));

CREATE TABLE child(pk INTEGER PRIMARY KEY, k INTEGER, fk INTEGER,
  FOREIGN KEY (fk) REFERENCES parent(j), UNIQUE KEY child_key(k));

CREATE TABLE grandchild(pk INTEGER PRIMARY KEY, fk INTEGER,
  FOREIGN KEY (fk) REFERENCES child(k));

SET @@foreign_key_checks= 0;
CREATE TABLE orphan_grandchild(pk INTEGER PRIMARY KEY, fk INTEGER,
  FOREIGN KEY (fk) REFERENCES siebling(k));
SET @@foreign_key_checks= 1;

CREATE TABLE non_atomic_t1(pk INTEGER) ENGINE= InnoDB;
CREATE TABLE non_atomic_t2(pk INTEGER) ENGINE= InnoDB;

--echo # FK definitions before rename:
SELECT name, unique_constraint_name, referenced_table_schema, referenced_table_name
  FROM mysql.foreign_keys
  WHERE referenced_table_schema LIKE 'test';

--error ER_TABLE_EXISTS_ERROR
RENAME TABLE child TO siebling,
 non_atomic_t1 TO non_atomic_t3,
 non_atomic_t3 TO non_atomic_t2;

--echo # After the rename, we see that the FK information remain unchanged.
--echo # TODO: Note that the unique constraint name of orphan_grandchild is not reverted.
SELECT name, unique_constraint_name, referenced_table_schema, referenced_table_name
  FROM mysql.foreign_keys
  WHERE referenced_table_schema LIKE 'test';

DROP TABLE grandchild;
DROP TABLE orphan_grandchild;
DROP TABLE child;
DROP TABLE parent;
DROP TABLE non_atomic_t1;
DROP TABLE non_atomic_t2;

# Restore defaults.
SET @@foreign_key_checks= DEFAULT;
SET SESSION debug= '-d,skip_dd_table_access_check';


--echo #
--echo # Additional test coverage for case when ALTER TABLE is non-atomic but
--echo # still adds foreign keys and fails at the late stage (after foreign
--echo # keys are added to the data-dictionary).
--echo #

--enable_connect_log
connect (con1, localhost, root,,);
SET @old_lock_wait_timeout= @@lock_wait_timeout;
connection default;

CREATE TABLE t1 (pk INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (fk INT) ENGINE=MyISAM;
SET @@debug='+d,injecting_fault_writing';
--replace_regex /(errno: .*)/(errno: #)/
--error ER_ERROR_ON_WRITE
ALTER TABLE t2 ADD FOREIGN KEY (fk) REFERENCES t1(pk), ENGINE=InnoDB;
SET @@debug='-d,injecting_fault_writing';
--echo # Even though the above ALTER TABLE has failed it has updated
--echo # data-dictionary. So cached object for parent table should
--echo # be updated to reflect this new foreign key.
--echo # The below LOCK TABLES should implicitly lock t2 too.
LOCK TABLES t1 WRITE;

connection con1;
--echo # So concurrent inserts into child are blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 VALUES (NULL);
SET @@lock_wait_timeout= @old_lock_wait_timeout;

connection default;
--echo # And delete from parent table is possible and doesn't cause asserts.
DELETE FROM t1;
UNLOCK TABLES;
DROP TABLES t2, t1;

connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;

--disable_connect_log
