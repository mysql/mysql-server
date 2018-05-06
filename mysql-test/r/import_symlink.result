#
# Testing WL#7524 - Import from SDI files using symlinks
#
# Setup test environment
#
# IM-POS-X3: Verify that a table created with the DATA DIRECTORY
# option can be imported, provided the symbolic link is restored
# manually.
#
# Create table with external DATA DIRECTORY
CREATE TABLE t1(i INT) ENGINE MYISAM DATA DIRECTORY "EXPORT_DIR";
INSERT INTO t1 VALUES (0), (2), (4);
# Make copies of all t1 files
DROP TABLE t1;
# Restore all t1 files and recreate symlink to external DATA DIRECTORY
IMPORT TABLE FROM 't1_*.sdi';
SELECT * FROM t1 ORDER BY i;
i
0
2
4
DROP TABLE t1;
# Clean $EXPORT_DIR
# Remove $EXPORT_DIR
