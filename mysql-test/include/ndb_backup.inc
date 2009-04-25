######################################################
# By JBM 2006-02-16 So that the code is not repeated #
# in test cases and can be reused.                   #
######################################################

--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "start backup" >> $NDB_TOOLS_OUTPUT

# To find the backupid, we must dump this data to a table, and SELECT
# what we want into an outfile.  This could be accomplished with grep, but
# grep isn't Windows-portable

--disable_query_log
# create a table to help us out
--disable_warnings # leave this on until done with the entire process
# cleanup
DROP TABLE IF EXISTS helper1;
CREATE TABLE helper1(c1 VARCHAR(20));
# dump raw data to file
let $ndb_backup_file1= $MYSQLTEST_VARDIR/ndb_backup_tmp.dat;
let $ndb_backup_file2= $MYSQLTEST_VARDIR/tmp.dat;
--error 0,1
--remove_file $ndb_backup_file1
--exec $NDB_TOOLS_DIR/ndb_select_all --ndb-connectstring="$NDB_CONNECTSTRING" -d sys --delimiter=',' SYSTAB_0 > $ndb_backup_file1
# load the table from the raw data file
eval LOAD DATA INFILE '$ndb_backup_file1' INTO TABLE helper1;
--remove_file $ndb_backup_file1
# output what we need
eval SELECT * FROM helper1 WHERE c1 LIKE '%520093696%'
INTO OUTFILE '$ndb_backup_file2';
# cleanup
DROP TABLE helper1;
--enable_warnings
--enable_query_log

CREATE TEMPORARY TABLE IF NOT EXISTS test.backup_info
(id INT, backup_id INT) ENGINE = MEMORY;

--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval LOAD DATA INFILE '$ndb_backup_file2' INTO TABLE test.backup_info FIELDS TERMINATED BY ',';
--remove_file $ndb_backup_file2

# Load backup id into environment variable
let the_backup_id=`SELECT  backup_id from test.backup_info`;

DROP TABLE test.backup_info;

