--source include/not_windows.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Testing WL#7524 - Import from SDI files using symlinks
--echo #

--echo # Setup test environment
let $MYSQLD_DATADIR=`SELECT @@datadir`;
--perl
chdir $ENV{'MYSQL_TMP_DIR'};
mkdir "export";
EOF
let $EXPORT_DIR= $MYSQL_TMP_DIR/export;

--echo #
--echo # IM-POS-X3: Verify that a table created with the DATA DIRECTORY
--echo # option can be imported, provided the symbolic link is restored
--echo # manually.
--echo #

--echo # Create table with external DATA DIRECTORY
--replace_regex /DIRECTORY.*/DIRECTORY "EXPORT_DIR"/
eval CREATE TABLE t1(i INT) ENGINE MYISAM DATA DIRECTORY "$EXPORT_DIR";
INSERT INTO t1 VALUES (0), (2), (4);
--echo # Make copies of all t1 files
--copy_file $EXPORT_DIR/t1.MYD  $EXPORT_DIR/_t1.MYD
--copy_files_wildcard $MYSQLD_DATADIR/test $EXPORT_DIR t1_*.sdi
--copy_file $MYSQLD_DATADIR/test/t1.MYI $MYSQLD_DATADIR/test/_t1.MYI
DROP TABLE t1;
--echo # Restore all t1 files and recreate symlink to external DATA DIRECTORY
--move_file $EXPORT_DIR/_t1.MYD  $EXPORT_DIR/t1.MYD
--move_file $MYSQLD_DATADIR/test/_t1.MYI $MYSQLD_DATADIR/test/t1.MYI
--exec ln -s $EXPORT_DIR/t1.MYD $MYSQLD_DATADIR/test/t1.MYD
--copy_files_wildcard $EXPORT_DIR $MYSQLD_DATADIR/test t1_*.sdi
IMPORT TABLE FROM 't1_*.sdi';
SELECT * FROM t1 ORDER BY i;
DROP TABLE t1;

--echo # Clean \$EXPORT_DIR
--remove_files_wildcard $EXPORT_DIR *
--echo # Remove \$EXPORT_DIR
--force-rmdir $EXPORT_DIR
