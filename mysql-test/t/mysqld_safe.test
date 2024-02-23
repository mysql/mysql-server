--source include/have_mysqld_safe.inc
--source include/not_windows.inc

# 1) Set variables to be used in parameters of mysqld_safe.
let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $MYSQL_BASEDIR= `SELECT @@basedir`;
let $MYSQL_SOCKET= `SELECT @@socket`;
let $MYSQL_TIMEZONE= `SELECT @@time_zone`;
let $MYSQL_PIDFILE= `SELECT @@pid_file`;
let $MYSQL_PORT= `SELECT @@port`;
let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`;
let $start_page_size= `select @@innodb_page_size`;
let $other_page_size_k=    `SELECT $start_page_size DIV 1024`;
let $other_page_size_nk=       `SELECT CONCAT($other_page_size_k,'k')`;

# mysqld_path to be passed to --ledir
use test;
perl;
 my $dir = $ENV{'MYSQLTEST_VARDIR'};
 open ( OUTPUT, ">$dir/tmp/mysqld_path_file.inc") ;
 my $path = $ENV{MYSQLD};
 $path =~ /^(.*)\/([^\/]*)$/;
 print OUTPUT "let \$mysqld_path = $1;\n";
 print OUTPUT "let \$mysqld_bin = $2;\n";
 close (OUTPUT);
EOF

# Get the value of the variable to MTR, from perl
--source  $MYSQLTEST_VARDIR/tmp/mysqld_path_file.inc

# Remove the temp file
--remove_file $MYSQLTEST_VARDIR/tmp/mysqld_path_file.inc

# 2) Shutdown mysqld which is started by mtr.
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "wait" > $_expect_file_name
--shutdown_server
--source include/wait_until_disconnected.inc

let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF

# Negative testing

# a) With non-existent basedir
--error 1
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR/nonexistent --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk


# b) With non-existent ledir
--error 1
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --ledir=$mysqld_path/nonexistent --datadir=$MYSQLTEST_VARDIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1

# c) With relative path to basedir
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR/./ --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &

# mysqld_safe takes some time to start mysqld
--source include/wait_until_connected_again.inc
--shutdown_server

# d) With relative path to ledir
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path/./ --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &

# mysqld_safe takes some time to start mysqld
--source include/wait_until_connected_again.inc
--shutdown_server

# e) With relative path to datadir
--exec $MYSQLD --no-defaults --loose-innodb-dedicated-server=OFF --initialize-insecure --init-file=$BOOTSTRAP_SQL --secure-file-priv="" --basedir=$MYSQL_BASEDIR --datadir=$MYSQLTEST_VARDIR/datatest1/
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLTEST_VARDIR/datatest1/./ --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &

 # mysqld_safe takes some time to start mysqld
--source include/wait_until_connected_again.inc
--shutdown_server

# f) Defaults file normal working
write_file $MYSQLTEST_VARDIR/tmp/defaultsfile.cnf;
[mysqld]
binlog_format=MIXED
EOF

--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/tmp/defaultsfile.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &

# mysqld_safe takes some time to start mysqld
--source include/wait_until_connected_again.inc

connection default;
--exec $MYSQL -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root -e "select @@binlog_format" 2>&1

--shutdown_server


# Uncomment after Bug#27347821 is fixed
# g) Defaults extra file normal working
# --exec sh $MYSQLD_SAFE --defaults-extra-file=$MYSQLTEST_VARDIR/tmp/defaultsfile.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &

#  mysqld_safe takes some time to start mysqld
# --source include/wait_until_connected_again.inc

#connection default;
#--exec $MYSQL -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root -e "select @@binlog_format" 2>&1
#--shutdown_server


# Uncomment the following 5 cases after Bug#27263741 is fixed
# h) With non-existent datadir
# --error 1
# --exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLTEST_VARDIR/nonexistent --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1

# i) With no permissions to datadir
# --exec $MYSQLD --no-defaults --loose-innodb-dedicated-server=OFF --initialize-insecure --init-file=$BOOTSTRAP_SQL --secure-file-priv="" --basedir=$MYSQL_BASEDIR --datadir=$MYSQLTEST_VARDIR/datatest2
#chmod 0666 $MYSQLTEST_VARDIR/datatest2;
# --error 1
# --exec sh $MYSQLD_SAFE --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLTEST_VARDIR/datatest2 --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1
# chmod 0777 $MYSQLTEST_VARDIR/datatest2;

# j) With non-existent defaults-file
# --error 1
# --exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/tmp/defaultsfilenon.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1

# k) With non-existent defaults-extra-file
# --error 1
# --exec sh $MYSQLD_SAFE --defaults-extra-file=$MYSQLTEST_VARDIR/tmp/defaultsxfilenon.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1

# l) With defaults-file as well as defaults-extra-file
# write_file $MYSQLTEST_VARDIR/tmp/defaultsxfile.cnf;
# [mysqld]
# max_connections=20
# EOF
# --error 1
# --exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/tmp/defaultsfile.cnf --defaults-extra-file=$MYSQLTEST_VARDIR/tmp/defaultsxfile.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1

# 3) Run the mysqld_safe script with exec.
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &
# mysqld_safe takes some time to start mysqld
--source include/wait_until_connected_again.inc

# 4) Reconnect to mysqld again
connection default;

# 5) Execute some SQL
--exec $MYSQL -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root -e "SHOW DATABASES" 2>&1

# 6) Kill mysqld, which must be restarted now automatically by mysqld_safe
--exec sh $MYSQL_TEST_DIR/t/mysqld_safe.sh $MYSQL_PIDFILE 2>&1

# mysqld_safe takes some time to restart mysqld
--source include/wait_until_connected_again.inc

# 7) Execute some SQL
--exec $MYSQL -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root -e "SHOW DATABASES" 2>&1

# 8) Shutdown mysqld with mysqladmin
--exec $MYSQLADMIN -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root shutdown 2>&1

# Delay introduced - mysqld_safe takes some time to restart mysqld
--source include/wait_until_disconnected.inc

# 9) Restart mysqld of mtr
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

# 10) Clean-up
--remove_file $BOOTSTRAP_SQL
--force-rmdir $MYSQLTEST_VARDIR/datatest1/
--remove_file $MYSQLTEST_VARDIR/tmp/defaultsfile.cnf
# Uncomment after Bug#27263741 is fixed
# --force-rmdir $MYSQLTEST_VARDIR/datatest2/
# --remove_file $MYSQLTEST_VARDIR/tmp/defaultsxfile.cnf
