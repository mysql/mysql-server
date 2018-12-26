# Include this at the end of any test using mysql_upgrade

let $MYSQLD_DATADIR= `select @@datadir`;

# mysql_upgrade should have created mysql_upgrade_info file,
# so the following command will do a clean-up.
--remove_file $MYSQLD_DATADIR/mysql_upgrade_info
