let $bootstrap_sql_file = $MYSQLTEST_VARDIR/tmp/bootstrap.sql;
write_file $bootstrap_sql_file;
CREATE DATABASE test;
EOF
let $MYSQLD_ARGS = --console --log-error-verbosity=3 --datadir=$MYSQLD_DATADIR $MYSQLD_EXTRA_ARGS > $MYSQLD_ERROR_LOG 2>&1 ;
--exec $MYSQLD_CMD $MYSQLD_ARGS --initialize-insecure --init-file=$bootstrap_sql_file
--remove_file $bootstrap_sql_file
