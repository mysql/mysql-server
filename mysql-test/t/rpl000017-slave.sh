rm -f $MYSQL_TEST_DIR/var/log/*relay*
rm -f $MYSQL_TEST_DIR/var/slave-data/relay-log.info
cat > $MYSQL_TEST_DIR/var/slave-data/master.info <<EOF
master-bin.000001
4
127.0.0.1
replicate
aaaaaaaaaaaaaaab
$MASTER_MYPORT
1
0
EOF
