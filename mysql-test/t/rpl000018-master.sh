rm -f $MYSQL_TEST_DIR/var/master-data/master-bin.* 
cp $MYSQL_TEST_DIR/std_data/master-bin.001  $MYSQL_TEST_DIR/var/master-data/
echo ./master-bin.001 >  $MYSQL_TEST_DIR/var/master-data/master-bin.index
