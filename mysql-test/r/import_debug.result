#
# Testing WL#7524 - Import from SDI files
#
# Tests needing debug binaries.
# Setup test environment
#
# IM-POS-2: Export, drop and import back table
#
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (1), (3), (5);
SELECT * FROM t1;
i
1
3
5
FLUSH TABLES WITH READ LOCK;
# Copy t1 files to export dir
UNLOCK TABLES;
DROP TABLE t1;
# Copy files back into datadir
# Make import target commit fail
SET SESSION debug= '+d,sdi_import_commit_fail';
IMPORT TABLE FROM 't1_*.sdi';
Warnings:
Warning	6	Error on delete of 'no_such_file' ...
SET SESSION debug= '-d,sdi_import_commit_fail';
DROP TABLE t1;
# Clean SDI files in $EXPORT_DIR and $MYSQLD_DATADIR
# Remove $EXPORT_DIR
