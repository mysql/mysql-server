rm -f $MYSQLTEST_VARDIR/log/*relay*
rm -f $MYSQLTEST_VARDIR/mysqld.2/data/relay-log.info
cat > $MYSQLTEST_VARDIR/mysqld.2/data/master.info <<EOF
master-bin.000001
4
127.0.0.1
replicate
aaaaaaaaaaaaaaab
$MASTER_MYPORT
1
0
EOF
