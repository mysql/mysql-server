# Remove anonymous users added by add_anonymous_users.inc
disable_warnings;
disable_query_log;
DELETE FROM mysql.user where host='localhost' and user='';
FLUSH PRIVILEGES;
enable_query_log;
enable_warnings;
