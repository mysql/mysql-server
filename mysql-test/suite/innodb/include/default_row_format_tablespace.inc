# Check Default row_format=Dynamic
SELECT @@innodb_default_row_format;

# Check if no tablespace is set
if ($tablespace == '') {

# Check Default file_per_table=ON & Barracuda
SELECT @@innodb_file_per_table;

--echo #Create table with no tablespace
CREATE TABLE tab(c1  TEXT, c2 BLOB);

# Insert some records
INSERT INTO tab VALUES('Check with no tablespace','Check with no tablespace');
}

# Check if tablespace is set
if ($tablespace == 'tblsp') {

# Check for compressed tables without File_block_size.
--echo #Create table with tablespace
CREATE TABLESPACE $tablespace ADD DATAFILE '$tablespace.ibd'
ENGINE=InnoDB;

CREATE TABLE tab(c1  TEXT, c2 BLOB) TABLESPACE $tablespace;

# Check for compressed tables with File_block_size.
CREATE TABLESPACE tblsp1 ADD DATAFILE 'tblsp1.ibd' FILE_BLOCK_SIZE=1k
ENGINE=InnoDB;
CREATE TABLE tab1(c1  TEXT, c2 BLOB) KEY_BLOCK_SIZE=1 TABLESPACE tblsp1;

# Insert some records
INSERT INTO tab VALUES('Check with General tablespace',
'Check with General tablespace');
INSERT INTO tab1 VALUES('tablsp File Block size',
'tablsp File Block size');
}

# Check if system tablespace is set
if ($tablespace == 'innodb_system') {

--echo #Create table with innodb system tablespace
# Create table in system tablesspace
CREATE TABLE tab(c1  TEXT, c2 BLOB) TABLESPACE innodb_system;

# Insert some records
INSERT INTO tab VALUES('Check with InnoDB system tablespace',
'Check with InnoDB system tablespace');

}

# Check when file_per_table=OFF is set
if ($tablespace == 'OFF') {
# Set file_per_table=OFF
SET GLOBAL innodb_file_per_table=0;

# Check Default file_per_table=OFF
SELECT @@innodb_file_per_table;

--echo #Create table with file_per_table=0
CREATE TABLE tab(c1  TEXT, c2 BLOB);

# Insert some records
INSERT INTO tab VALUES('File per table off','File per table off');
}

# Check Default row_format=Dynamic
--source suite/innodb/include/default_row_format_show.inc

ALTER TABLE tab ROW_FORMAT=COMPACT;

# Check Default row_format=Compact
--source suite/innodb/include/default_row_format_show.inc

CHECK TABLE tab;

ALTER TABLE tab ROW_FORMAT=DYNAMIC;

# Check Default row_format=Dynamic
--source suite/innodb/include/default_row_format_show.inc

CHECK TABLE tab;

ALTER TABLE tab ROW_FORMAT=REDUNDANT;

# Check Default row_format=Redundant
--source suite/innodb/include/default_row_format_show.inc

CHECK TABLE tab;

# Check successful, when no tablespace is set
if ($tablespace == '') {
ALTER TABLE tab ROW_FORMAT=COMPRESSED;
}

# When tablespace is set, check error for compressed tables with
# and without File_block_size
# Check ALTER success for compressed tables with tablespace File_block_size
if ($tablespace == 'tblsp') {

# Check error with compressed tables without File_block_size
--error ER_ILLEGAL_HA_CREATE_OPTION
ALTER TABLE tab ROW_FORMAT=COMPRESSED;

# Check error with compressed tables with File_block_size
--error ER_ILLEGAL_HA_CREATE_OPTION
ALTER TABLE tab1 ROW_FORMAT=DYNAMIC;
}

# Check when file_per_table=OFF is set
if ($tablespace == 'OFF') {
--error ER_ILLEGAL_HA_CREATE_OPTION
ALTER TABLE tab ROW_FORMAT=COMPRESSED;
}

# Check Default row_format=Compressed
# Note: we do not use default_row_format_show.inc because
# ALTER ROW_FORMAT=COMPRESSED shows different compressed page sizes
# with different innodb_page_sizes
SELECT NAME,FILE_FORMAT,ROW_FORMAT FROM INFORMATION_SCHEMA.INNODB_SYS_TABLES
WHERE NAME='test/tab';

CHECK TABLE tab;

if ($tablespace == 'tblsp') {
CHECK TABLE tab1;
}

ALTER TABLE tab ROW_FORMAT=Dynamic;

# Check Default row_format=Dynamic;
--source suite/innodb/include/default_row_format_show.inc

CHECK TABLE tab;

# Cleanup
DROP TABLE tab;

# Drop only when tablespace is set
if ($tablespace == 'tblsp') {
DROP TABLE tab1;
DROP TABLESPACE $tablespace;
DROP TABLESPACE tblsp1;
}

# Reset to default values
SET GLOBAL innodb_file_per_table=Default;
