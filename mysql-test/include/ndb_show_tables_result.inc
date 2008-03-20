--disable_warnings
--disable_query_log
CREATE TEMPORARY TABLE IF NOT EXISTS ndb_show_tables_results (id INT, type VARCHAR(20), state VARCHAR(20), logging VARCHAR(20), _database VARCHAR(255), _schema VARCHAR(20), name VARCHAR(255));

--exec $NDB_TOOLS_DIR/ndb_show_tables --p > $MYSQLTEST_VARDIR/master-data/test/tmp.dat
LOAD DATA INFILE 'tmp.dat' INTO TABLE ndb_show_tables_results;
--enable_query_log
--enable_warnings
--echo
--echo ndb_show_tables completed.....
--echo
