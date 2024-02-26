# Prepare command set of DML and DDL commands that are run
# when a concurrent connection has read ACL table. This file
# dml_ddl.inc is used below in several test cases.

# Print locks acquired on row 'u2', 'u2x'.
eval $query_row_locks;

# Delete the user 'u2'
DELETE FROM mysql.user WHERE User='u2';

# Insert the user 'u2x' from backup.
INSERT INTO mysql.user SELECT * FROM user_bk;

# Replace the user 'u2x' from backup.
REPLACE INTO mysql.user SELECT * FROM user_bk;

# Rename 'u2x' to 'u2'.
UPDATE mysql.user SET user='u2' WHERE user LIKE 'u2x';

# Alter the user 'u2' using DDL
let newpass=`SELECT SUBSTRING((lpad(hex(round(rand() * 10000000)),6,0)),-6)`;
--disable_query_log
eval ALTER USER u2 IDENTIFIED BY '$newpass';
--enable_query_log

# Execute DDL changing mysql.user.
CREATE USER someuser;
ALTER USER someuser IDENTIFIED BY 'somepass';
DROP USER someuser;
