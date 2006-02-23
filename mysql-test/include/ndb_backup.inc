######################################################
# By JBM 2006-02-16 So that the code is not repeated #
# in test cases and can be reused.                   #
######################################################
--exec $NDB_MGM --no-defaults --ndb-connectstring="localhost:$NDBCLUSTER_PORT" -e "start backup" >> $NDB_TOOLS_OUTPUT

# there is no neat way to find the backupid, this is a hack to find it...

--exec $NDB_TOOLS_DIR/ndb_select_all --ndb-connectstring="localhost:$NDBCLUSTER_PORT" -d sys --delimiter=',' SYSTAB_0 | grep 520093696 > $MYSQLTEST_VARDIR/tmp.dat

CREATE TEMPORARY TABLE IF NOT EXISTS test.backup_info (id INT, backup_id INT) ENGINE = HEAP;

DELETE FROM test.backup_info;

LOAD DATA INFILE '../tmp.dat' INTO TABLE test.backup_info FIELDS TERMINATED BY ',';

--replace_column 1 <the_backup_id>

SELECT @the_backup_id:=backup_id FROM test.backup_info;

let the_backup_id=`select @the_backup_id`;

DROP TABLE test.backup_info;

