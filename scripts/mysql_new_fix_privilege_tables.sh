#!/bin/sh

echo "This scripts updates the mysql.user, mysql.db, mysql.host and the"
echo "mysql.func table to MySQL 3.22.14 and above."
echo ""
echo "This is needed if you want to use the new GRANT functions,"
echo "CREATE AGGREAGATE FUNCTION or want to use the more secure passwords in 3.23"
echo ""
echo "If you get Access denied errors, you should run this script again"
echo "and give the MySQL root user password as a argument!"

root_password="$1"
host="localhost"

# Fix old password format, add File_priv and func table
echo ""
echo "If your tables are already up to date or partially up to date you will"
echo "get some warnings about 'Duplicated column name'. You can safely ignore these!"

@bindir@/mysql -f --user=root --password="$root_password" --host="$host" mysql <<END_OF_DATA
alter table user add max_questions int(11) unsigned DEFAULT 0  NOT NULL;
alter table user add max_updates int(11) unsigned DEFAULT 0  NOT NULL;
alter table user add max_connections int(11) unsigned DEFAULT 0  NOT NULL;
END_OF_DATA
