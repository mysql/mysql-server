rm -f $MYSQL_TEST_DIR/var/lib/master-bin.* 
cp $MYSQL_TEST_DIR/std_data/master-bin.001  $MYSQL_TEST_DIR/var/lib/
echo ./master-bin.001 >  $MYSQL_TEST_DIR/var/lib/master-bin.index
