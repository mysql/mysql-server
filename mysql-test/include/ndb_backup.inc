######################################################
# By JBM 2006-02-16 So that the code is not repeated #
# in test cases and can be reused.                   #
######################################################
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "start backup" >> $NDB_TOOLS_OUTPUT

# there is no neat way to find the backupid, this is a hack to find it...
let $dump_file= $MYSQLTEST_VARDIR/tmp/tmp.dat;
--exec $NDB_TOOLS_DIR/ndb_select_all --ndb-connectstring="$NDB_CONNECTSTRING" -d sys --delimiter=',' SYSTAB_0 | grep 520093696 > $dump_file

CREATE TEMPORARY TABLE test.backup_info (id INT, backup_id INT) ENGINE = HEAP;

--replace_result $dump_file DUMP_FILE
eval LOAD DATA INFILE '$dump_file' INTO TABLE test.backup_info FIELDS TERMINATED BY ',';

# Load backup id into environment variable
let the_backup_id=`SELECT  backup_id from test.backup_info`;
DROP TABLE test.backup_info;

remove_file $dump_file;


