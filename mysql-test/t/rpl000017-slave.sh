cat > $MYSQL_TEST_DIR/var/slave-data/master.info <<EOF
master-bin.001
4
127.0.0.1
replicate
aaaaaaaaaaaaaaabthispartofthepasswordisnotused
$MASTER_MYPORT
1
0
EOF
