--disable_warnings
--disable_query_log
CREATE TEMPORARY TABLE IF NOT EXISTS ndb_show_tables_results (id INT, type VARCHAR(20), state VARCHAR(20), logging VARCHAR(20), _database VARCHAR(255), _schema VARCHAR(20), name VARCHAR(255));

--let $MYSQLD_DATADIR= `SELECT @@datadir`
--exec $NDB_TOOLS_DIR/ndb_show_tables --p > $MYSQLD_DATADIR/test/tmp.dat
LOAD DATA INFILE 'tmp.dat' INTO TABLE ndb_show_tables_results;
--exec rm $MYSQLD_DATADIR/test/tmp.dat
--enable_query_log
--enable_warnings
--echo
--echo ndb_show_tables completed.....
--echo
