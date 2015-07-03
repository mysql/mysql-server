# Remember some variables.
LET $MYSQLD_DATADIR = `select @@datadir`;
LET $INNODB_PAGE_SIZE = `select @@innodb_page_size`;

# Create some relative remote directories
--mkdir $MYSQLD_DATADIR/../spaces
--mkdir $MYSQLD_DATADIR/../remote
--mkdir $MYSQL_TMP_DIR/portability
--mkdir $MYSQL_TMP_DIR/portability/tables
--mkdir $MYSQL_TMP_DIR/portability/spaces
--mkdir $MYSQL_TMP_DIR/portability/altogether

--echo #
--echo # Create the sample database to test for portability.
--echo #

CREATE TABLE ibport_t1 (a INT, b varchar(30), c int, key ka(a), key kb(b)) ENGINE=InnoDB;
INSERT INTO ibport_t1 VALUES (1, 'One', 11), (2, 'two', 22), (3, 'three', 33), (4, 'four', 44);

CREATE TABLESPACE ibport_s1 ADD DATAFILE 'ibport_s1_gen.ibd' ENGINE=InnoDB;
CREATE TABLE ibport_t5 (a INT, b varchar(30), c int, key ka(a), key kb(b)) ENGINE=InnoDB TABLESPACE ibport_s1;
INSERT INTO ibport_t5 VALUES (1, 'One', 11), (2, 'two', 22), (3, 'three', 33), (4, 'four', 44);

CREATE TABLESPACE ibport_s2 ADD DATAFILE '../spaces/ibport_s2_gen.ibd' ENGINE=InnoDB FILE_BLOCK_SIZE=4k;
CREATE TABLE ibport_t6 (a INT, b varchar(30), c int, key ka(a), key kb(b)) ENGINE=InnoDB TABLESPACE ibport_s2
  ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=4;
INSERT INTO ibport_t6 VALUES (1, 'One', 11), (2, 'two', 22), (3, 'three', 33), (4, 'four', 44);

--replace_result  #P# #p#  #SP# #sp#
SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.innodb_sys_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

--replace_result  #P# #p#  #SP# #sp#
SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.frm
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/test/ ibport*.frm

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.isl
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/test/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.ibd
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/test/ ibport*.ibd

--echo # Directory listing of MYSQLD_DATADIR/ ibport*.isl
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/ ibport*.ibd
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/ ibport*.ibd

--echo # Directory listing of ../spaces/ ibport*
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/../spaces/ ibport*

--echo # Add some uncommitted records to these tablespaces before clean shutdown
INSERT INTO ibport_t1 VALUES (5, 'Five', 55);
INSERT INTO ibport_t5 VALUES (5, 'Five', 55);
INSERT INTO ibport_t6 VALUES (5, 'Five', 55);
BEGIN;
UPDATE ibport_t1 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t5 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t6 SET a = 99, b = 'Uncommitted' WHERE a < 3;
DELETE FROM ibport_t1 WHERE a = 3;
DELETE FROM ibport_t5 WHERE a = 3;
DELETE FROM ibport_t6 WHERE a = 3;

--echo #
--echo # Shutdown the server and replace the relative paths in the ISL files with
--echo # full paths to these locations.  This makes the SYS_DATAFILES.PATH and the
--echo # ISL path look different but point to the same place.  InnoDB should
--echo # recognize that they point to the same file and update SYS_DATAFILES.PATH.
--echo #
--source include/shutdown_mysqld.inc

--echo # Put the full path to the *.ibd files into *.isl files.
--exec echo $MYSQLD_DATADIR/ibport_s1_gen.ibd                       > $MYSQLD_DATADIR/ibport_s1_gen.isl
--exec echo $MYSQLD_DATADIR\\..spaces\\ibport_s2_gen.ibd            > $MYSQLD_DATADIR/ibport_s2_gen.isl
--exec echo $MYSQLD_DATADIR/test\\ibport_t1.ibd                     > $MYSQLD_DATADIR/test/ibport_t1.isl

--echo # Restart the DB server.
--echo # Show that the tables are open and the full paths are in SYS_DATAFILES.
--source include/start_mysqld.inc

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.frm
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/test/ ibport*.frm

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.isl
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/test/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.ibd
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/test/ ibport*.ibd

--echo # Directory listing of MYSQLD_DATADIR/ ibport*.isl
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/ ibport*.ibd
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/ ibport*.ibd

--echo # Directory listing of ../spaces/ ibport*
--replace_result  #P# #p#  #SP# #sp#
--list_files $MYSQLD_DATADIR/../spaces/ ibport*

SELECT * FROM ibport_t1 ORDER BY a;
SELECT * FROM ibport_t5 ORDER BY a;
SELECT * FROM ibport_t6 ORDER BY a;

--replace_result  #P# #p#  #SP# #sp#  $MYSQLD_DATADIR MYSQLD_DATADIR
SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.innodb_sys_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

--replace_result  #P# #p#  #SP# #sp#  $MYSQLD_DATADIR MYSQLD_DATADIR
SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

SHOW CREATE TABLE ibport_t1;
SHOW CREATE TABLE ibport_t5;
SHOW CREATE TABLE ibport_t6;

--echo # Add some uncommitted records to these tablespaces before clean shutdown
INSERT INTO ibport_t1 VALUES (6, 'Six', 66);
INSERT INTO ibport_t5 VALUES (6, 'Six', 66);
INSERT INTO ibport_t6 VALUES (6, 'Six', 66);
BEGIN;
UPDATE ibport_t1 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t5 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t6 SET a = 99, b = 'Uncommitted' WHERE a < 3;
DELETE FROM ibport_t1 WHERE a = 3;
DELETE FROM ibport_t5 WHERE a = 3;
DELETE FROM ibport_t6 WHERE a = 3;

--echo # Stop server
--source include/shutdown_mysqld.inc

--echo #
--echo # Move all the datafiles to a new location and update the isl files.
--echo #

--move_file $MYSQLD_DATADIR/test/ibport_t1.ibd                         $MYSQL_TMP_DIR/portability/tables/ibport_t1.ibd
--move_file $MYSQLD_DATADIR/ibport_s1_gen.ibd                          $MYSQL_TMP_DIR/portability/spaces/ibport_s1_gen.ibd
--move_file $MYSQLD_DATADIR/../spaces/ibport_s2_gen.ibd                $MYSQL_TMP_DIR/portability/spaces/ibport_s2_gen.ibd

--echo # Replace the *.isl files with the full path to the *.ibd files
--exec echo $MYSQL_TMP_DIR/portability/tables\\ibport_t1.ibd            > $MYSQLD_DATADIR/test/ibport_t1.isl
--exec echo $MYSQL_TMP_DIR/portability\\spaces/ibport_s1_gen.ibd        > $MYSQLD_DATADIR/ibport_s1_gen.isl
--exec echo $MYSQL_TMP_DIR\\portability/spaces\\ibport_s2_gen.ibd       > $MYSQLD_DATADIR/ibport_s2_gen.isl

--echo # Directory listing of MYSQLD_DATADIR/ ibport*
--list_files $MYSQLD_DATADIR/ ibport*
--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.frm
--list_files $MYSQLD_DATADIR/test/ ibport*.frm
--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.isl
--list_files $MYSQLD_DATADIR/test/ ibport*.isl
--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.ibd
--list_files $MYSQLD_DATADIR/test/ ibport*.ibd
--echo # Directory listing of MYSQL_TMP_DIR/portability/tables/ ibport*
--list_files $MYSQL_TMP_DIR/portability/tables/ ibport*
--echo # Directory listing of MYSQL_TMP_DIR/portability/spaces/ ibport*
--list_files $MYSQL_TMP_DIR/portability/spaces/ ibport*

--echo # Start the DB server
--source include/start_mysqld.inc

SELECT * FROM ibport_t1 ORDER BY a;
SELECT * FROM ibport_t5 ORDER BY a;
SELECT * FROM ibport_t6 ORDER BY a;

--replace_result  $MYSQL_TMP_DIR MYSQL_TMP_DIR
SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.innodb_sys_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

--replace_result  $MYSQL_TMP_DIR MYSQL_TMP_DIR
SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

--replace_result  $MYSQL_TMP_DIR MYSQL_TMP_DIR
SHOW CREATE TABLE ibport_t1;
SHOW CREATE TABLE ibport_t5;
SHOW CREATE TABLE ibport_t6;

--echo # Add some uncommitted records to these tablespaces before clean shutdown
INSERT INTO ibport_t1 VALUES (7, 'Seven', 77);
INSERT INTO ibport_t5 VALUES (7, 'Seven', 77);
INSERT INTO ibport_t6 VALUES (7, 'Seven', 77);
BEGIN;
UPDATE ibport_t1 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t5 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t6 SET a = 99, b = 'Uncommitted' WHERE a < 3;
DELETE FROM ibport_t1 WHERE a = 3;
DELETE FROM ibport_t5 WHERE a = 3;
DELETE FROM ibport_t6 WHERE a = 3;

--echo #
--echo # Shutdown the server again and move each of the tablespaces to
--echo # a common location and update the ISL files accordingly.
--echo #
--source include/shutdown_mysqld.inc

--move_file $MYSQL_TMP_DIR/portability/tables/ibport_t1.ibd                $MYSQL_TMP_DIR/portability/altogether/ibport_t1.ibd
--move_file $MYSQL_TMP_DIR/portability/spaces/ibport_s1_gen.ibd            $MYSQL_TMP_DIR/portability/altogether/ibport_s1_gen.ibd
--move_file $MYSQL_TMP_DIR/portability/spaces/ibport_s2_gen.ibd            $MYSQL_TMP_DIR/portability/altogether/ibport_s2_gen.ibd

--exec echo $MYSQL_TMP_DIR\\portability/altogether/ibport_t1.ibd            > $MYSQLD_DATADIR/test/ibport_t1.isl
--exec echo $MYSQL_TMP_DIR\\portability/altogether\\ibport_s1_gen.ibd        > $MYSQLD_DATADIR/ibport_s1_gen.isl
--exec echo $MYSQL_TMP_DIR/portability/altogether\\ibport_s2_gen.ibd        > $MYSQLD_DATADIR/ibport_s2_gen.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.frm
--list_files $MYSQLD_DATADIR/test/ ibport*.frm
--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.isl
--list_files $MYSQLD_DATADIR/test/ ibport*.isl
--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.ibd
--list_files $MYSQLD_DATADIR/test/ ibport*.ibd
--echo # Directory listing of MYSQL_TMP_DIR/portability/altogether/
--list_files $MYSQL_TMP_DIR/portability/altogether/

--echo # Start the DB server
--source include/start_mysqld.inc

--replace_result  $MYSQL_TMP_DIR MYSQL_TMP_DIR
SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.innodb_sys_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

--replace_result  $MYSQL_TMP_DIR MYSQL_TMP_DIR
SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

--replace_result  $MYSQL_TMP_DIR MYSQL_TMP_DIR
SHOW CREATE TABLE ibport_t1;
SHOW CREATE TABLE ibport_t5;
SHOW CREATE TABLE ibport_t6;

SELECT * FROM ibport_t1 ORDER BY a;
SELECT * FROM ibport_t5 ORDER BY a;
SELECT * FROM ibport_t6 ORDER BY a;

--echo # Add some uncommitted records to these tablespaces before clean shutdown
INSERT INTO ibport_t1 VALUES (8, 'Eight', 88);
INSERT INTO ibport_t5 VALUES (8, 'Eight', 88);
INSERT INTO ibport_t6 VALUES (8, 'Eight', 88);
BEGIN;
UPDATE ibport_t1 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t5 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t6 SET a = 99, b = 'Uncommitted' WHERE a < 3;
DELETE FROM ibport_t1 WHERE a = 3;
DELETE FROM ibport_t5 WHERE a = 3;
DELETE FROM ibport_t6 WHERE a = 3;

--echo #
--echo # Shutdown the server again and move each of the tablespaces to their
--echo # default locations.  Leave the ISL files and the dictionary pointing to
--echo # the MYSQL_TMP_DIR/portability/altogether location.  The dictionary
--echo # will be adjusted when the files are found in the default locations.
--echo # The ISL files will be ignored.
--echo #
--source include/shutdown_mysqld.inc

--move_file $MYSQL_TMP_DIR/portability/altogether/ibport_t1.ibd             $MYSQLD_DATADIR/test/ibport_t1.ibd
--move_file $MYSQL_TMP_DIR/portability/altogether/ibport_s1_gen.ibd         $MYSQLD_DATADIR/ibport_s1_gen.ibd
--move_file $MYSQL_TMP_DIR/portability/altogether/ibport_s2_gen.ibd         $MYSQLD_DATADIR/ibport_s2_gen.ibd

--echo # Directory listing of MYSQLD_DATADIR/ ibport*.ibd
--list_files $MYSQLD_DATADIR/ ibport*.ibd

--echo # Directory listing of MYSQLD_DATADIR/ ibport*.isl
--list_files $MYSQLD_DATADIR/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.frm
--list_files $MYSQLD_DATADIR/test/ ibport*.frm

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.isl
--list_files $MYSQLD_DATADIR/test/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.ibd
--list_files $MYSQLD_DATADIR/test/ ibport*.ibd

--echo # Directory listing of MYSQL_TMP_DIR/portability/altogether/
--list_files $MYSQL_TMP_DIR/portability/altogether/

--echo # Start the DB server
--source include/start_mysqld.inc

SHOW CREATE TABLE ibport_t1;
SHOW CREATE TABLE ibport_t5;
SHOW CREATE TABLE ibport_t6;

SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.innodb_sys_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

SELECT * FROM ibport_t1 ORDER BY a;
SELECT * FROM ibport_t5 ORDER BY a;
SELECT * FROM ibport_t6 ORDER BY a;

--echo # Add some uncommitted records to these tablespaces before clean shutdown
INSERT INTO ibport_t1 VALUES (9, 'Nine', 99);
INSERT INTO ibport_t5 VALUES (9, 'Nine', 99);
INSERT INTO ibport_t6 VALUES (9, 'Nine', 99);
BEGIN;
UPDATE ibport_t1 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t5 SET a = 99, b = 'Uncommitted' WHERE a < 3;
UPDATE ibport_t6 SET a = 99, b = 'Uncommitted' WHERE a < 3;
DELETE FROM ibport_t1 WHERE a = 3;
DELETE FROM ibport_t5 WHERE a = 3;
DELETE FROM ibport_t6 WHERE a = 3;

--echo #
--echo # Shutdown the server again and move each of the tablespaces to the
--echo # original locations.
--echo # Make the ISL files point to these locations. using relative pathnames.
--echo # The dictionary is pointing to the default locations and will be adjusted.
--echo #
--source include/shutdown_mysqld.inc

--move_file $MYSQLD_DATADIR/ibport_s2_gen.ibd             $MYSQLD_DATADIR/../spaces/ibport_s2_gen.ibd
--remove_file $MYSQLD_DATADIR/test/ibport_t1.isl

--exec echo ..\\spaces/ibport_s2_gen.ibd                   > $MYSQLD_DATADIR/ibport_s2_gen.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.frm
--list_files $MYSQLD_DATADIR/test/ ibport*.frm

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.isl
--list_files $MYSQLD_DATADIR/test/ ibport*.isl

--echo # Directory listing of MYSQLD_DATADIR/test/ ibport*.ibd
--list_files $MYSQLD_DATADIR/test/ ibport*.ibd

--echo # Start the DB server
--source include/start_mysqld.inc

SELECT * FROM ibport_t1 ORDER BY a;
SELECT * FROM ibport_t5 ORDER BY a;
SELECT * FROM ibport_t6 ORDER BY a;

SHOW CREATE TABLE ibport_t1;
SHOW CREATE TABLE ibport_t5;
SHOW CREATE TABLE ibport_t6;

SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.innodb_sys_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_sys_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

--echo #
--echo # Cleanup
--echo #

DROP TABLE ibport_t1;
DROP TABLE ibport_t5;
DROP TABLE ibport_t6;
DROP TABLESPACE ibport_s1;
DROP TABLESPACE ibport_s2;
--rmdir $MYSQLD_DATADIR/../spaces
--rmdir $MYSQLD_DATADIR/../remote
--rmdir $MYSQL_TMP_DIR/portability/altogether
--rmdir $MYSQL_TMP_DIR/portability/tables
--rmdir $MYSQL_TMP_DIR/portability/spaces
--rmdir $MYSQL_TMP_DIR/portability

--disable_query_log
call mtr.add_suppression("\\[ERROR\\] InnoDB: Operating system error number .* in a file operation");
call mtr.add_suppression("\\[ERROR\\] InnoDB: Cannot open datafile for read-only:");
call mtr.add_suppression("\\[ERROR\\] InnoDB: A link file was found named '.*isl' but the linked tablespace '.*ibd' could not be opened read-only");
call mtr.add_suppression("\\[ERROR\\] InnoDB: The error means the system cannot find the path specified");
call mtr.add_suppression("\\[ERROR\\] InnoDB: If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them");
--enable_query_log
