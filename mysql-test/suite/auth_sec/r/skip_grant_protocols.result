
# Set different paths for --datadir

# Create custom datadir path

# Run the bootstrap command to create a new datadir

# Start the DB server with the new datadir
# restart: --datadir=MYSQLD_DATADIR1
SELECT current_user;
current_user
root@localhost
SELECT user, plugin FROM mysql.user;
user	plugin
mysql.infoschema	caching_sha2_password
mysql.session	caching_sha2_password
mysql.sys	caching_sha2_password
root	caching_sha2_password

# Set the password for the root user
SET PASSWORD = 'passwd';

# Create a new user with non-default plugin
CREATE USER 'native_user' IDENTIFIED WITH 'mysql_native_password' BY 'passwd';

# Restart server with --skip-grant-tables option and new datadir
# restart: --skip-grant-tables --datadir=MYSQLD_DATADIR1

# connect the root user which uses the default auth plugin
connection_type
Socket
current_user
skip-grants user@skip-grants host

# connect the native_user plugin user which uses the default auth plugin
connection_type
Socket
current_user
skip-grants user@skip-grants host

# connect the root user which uses the non-default auth plugin
connection_type
Socket
current_user
skip-grants user@skip-grants host

# connect the native user plugin user which uses corresponding plugin
connection_type
Socket
current_user
skip-grants user@skip-grants host

# Cleanup
# restart
