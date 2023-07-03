CALL mtr.add_suppression("Storage engine '.*' does not support system tables. \\[mysql.*\\]");
CALL mtr.add_suppression("Column count of mysql.* is wrong. "
                         "Expected .*, found .*. "
                         "The table is probably corrupted");
CALL mtr.add_suppression("Cannot load from mysql.*. The table is probably corrupted");
#
# WL7158: Move privilege system tables from MyISAM to InnoDB
#
SET autocommit= 0;
SET innodb_lock_wait_timeout= 1;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host

# Connection: con1
SET autocommit= 0;
SET innodb_lock_wait_timeout= 1;

# Connection: default

########################################################################
# A.1. Checking lock-wait-timeout with CREATE USER.
########################################################################
START TRANSACTION;
INSERT INTO mysql.user(user, host, ssl_cipher, x509_issuer, x509_subject)
VALUES ('u2', 'h', '', '', '');

# Connection: con1
CREATE USER u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.2. Checking lock-wait-timeout with ALTER USER.
########################################################################
CREATE USER u1@h, u2@h, u3@h;
SELECT user, host, password_lifetime FROM mysql.user WHERE host = 'h';
user	host	password_lifetime
u1	h	NULL
u2	h	NULL
u3	h	NULL
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
ALTER USER u1@h, u2@h, u3@h PASSWORD EXPIRE INTERVAL 250 DAY;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, host, password_lifetime FROM mysql.user WHERE host = 'h';
user	host	password_lifetime
u1	h	250
u2	h	250
u3	h	250
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.3. Checking lock-wait-timeout with RENAME USER.
########################################################################
CREATE USER u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
RENAME USER u1@h TO u1a@h, u2@h TO u2a@h, u3@h TO u3a@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1a	h
u2a	h
u3a	h
DROP USER u1a@h, u2a@h, u3a@h;

########################################################################
# A.4. Checking lock-wait-timeout with SET PASSWORD.
########################################################################
CREATE USER u1@h IDENTIFIED WITH 'mysql_native_password';
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u1' AND host = 'h' FOR UPDATE;
user	host
u1	h

# Connection: con1
SET PASSWORD FOR u1@h = 'xxx';

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, host, authentication_string = '*3D56A309CD04FA2EEF181462E59011F075C89548' FROM mysql.user WHERE host = 'h';
user	host	authentication_string = '*3D56A309CD04FA2EEF181462E59011F075C89548'
u1	h	1
DROP USER u1@h;

########################################################################
# A.5. Checking lock-wait-timeout with DROP USER.
########################################################################
CREATE USER u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
DROP USER u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host

########################################################################
# A.6.1. Checking lock-wait-timeout with GRANT (global level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
GRANT SELECT ON *.* TO u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, select_priv FROM mysql.user WHERE host = 'h';
user	select_priv
u1	Y
u2	Y
u3	Y
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.6.2. Checking lock-wait-timeout with GRANT (database level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
GRANT SELECT ON test.* TO u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT * FROM mysql.db WHERE host = 'h';
Host	Db	User	Select_priv	Insert_priv	Update_priv	Delete_priv	Create_priv	Drop_priv	Grant_priv	References_priv	Index_priv	Alter_priv	Create_tmp_table_priv	Lock_tables_priv	Create_view_priv	Show_view_priv	Create_routine_priv	Alter_routine_priv	Execute_priv	Event_priv	Trigger_priv
h	test	u1	Y	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N
h	test	u2	Y	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N
h	test	u3	Y	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N	N
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.6.3. Checking lock-wait-timeout with GRANT (table level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
CREATE TABLE t1(a INT);
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
GRANT SELECT ON t1 TO u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, table_name, table_priv FROM mysql.tables_priv WHERE host = 'h';
user	table_name	table_priv
u1	t1	Select
u2	t1	Select
u3	t1	Select
DROP USER u1@h, u2@h, u3@h;
DROP TABLE t1;

########################################################################
# A.6.4. Checking lock-wait-timeout with GRANT (column level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
CREATE TABLE t1(a INT);
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
GRANT SELECT(a) ON t1 TO u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, column_name, column_priv FROM mysql.columns_priv WHERE host = 'h';
user	column_name	column_priv
u1	a	Select
u2	a	Select
u3	a	Select
DROP USER u1@h, u2@h, u3@h;
DROP TABLE t1;

########################################################################
# A.6.5. Checking lock-wait-timeout with GRANT (stored routine level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
CREATE PROCEDURE p1() SELECT 1;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
GRANT EXECUTE ON PROCEDURE p1 TO u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, routine_name, proc_priv FROM mysql.procs_priv WHERE host = 'h';
user	routine_name	proc_priv
u1	p1	Execute
u2	p1	Execute
u3	p1	Execute
DROP USER u1@h, u2@h, u3@h;
DROP PROCEDURE p1;

########################################################################
# A.7.1. Checking lock-wait-timeout with REVOKE (global level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT ON *.* TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE SELECT ON *.* FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, select_priv FROM mysql.user WHERE host = 'h';
user	select_priv
u1	N
u2	N
u3	N
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.7.2. Checking lock-wait-timeout with REVOKE (database level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT ON test.* TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE SELECT ON test.* FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT * FROM mysql.db WHERE host = 'h';
Host	Db	User	Select_priv	Insert_priv	Update_priv	Delete_priv	Create_priv	Drop_priv	Grant_priv	References_priv	Index_priv	Alter_priv	Create_tmp_table_priv	Lock_tables_priv	Create_view_priv	Show_view_priv	Create_routine_priv	Alter_routine_priv	Execute_priv	Event_priv	Trigger_priv
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.7.3. Checking lock-wait-timeout with REVOKE (table level).
########################################################################
CREATE TABLE t1(a INT);
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT ON t1 TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE SELECT ON t1 FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT user, table_name, table_priv FROM mysql.tables_priv WHERE host = 'h';
user	table_name	table_priv
DROP USER u1@h, u2@h, u3@h;
DROP TABLE t1;

########################################################################
# A.7.4. Checking lock-wait-timeout with REVOKE (column level).
########################################################################
CREATE TABLE t1(a INT);
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT(a) ON t1 TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE SELECT(a) ON t1 FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT user, column_name, column_priv FROM mysql.columns_priv WHERE host = 'h';
user	column_name	column_priv
DROP USER u1@h, u2@h, u3@h;
DROP TABLE t1;

########################################################################
# A.7.5. Checking lock-wait-timeout with REVOKE (stored routine level).
########################################################################
CREATE PROCEDURE p1() SELECT 1;
CREATE USER u1@h, u2@h, u3@h;
GRANT EXECUTE ON PROCEDURE p1 TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE EXECUTE ON PROCEDURE p1 FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT user, routine_name, proc_priv FROM mysql.procs_priv WHERE host = 'h';
user	routine_name	proc_priv
DROP USER u1@h, u2@h, u3@h;
DROP PROCEDURE p1;

########################################################################
# A.8.1. Checking lock-wait-timeout with REVOKE ALL (global level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT ON *.* TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user, select_priv FROM mysql.user WHERE host = 'h';
user	select_priv
u1	N
u2	N
u3	N
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.8.2. Checking lock-wait-timeout with REVOKE ALL (database level).
########################################################################
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT ON test.* TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT * FROM mysql.db WHERE host = 'h';
Host	Db	User	Select_priv	Insert_priv	Update_priv	Delete_priv	Create_priv	Drop_priv	Grant_priv	References_priv	Index_priv	Alter_priv	Create_tmp_table_priv	Lock_tables_priv	Create_view_priv	Show_view_priv	Create_routine_priv	Alter_routine_priv	Execute_priv	Event_priv	Trigger_priv
DROP USER u1@h, u2@h, u3@h;

########################################################################
# A.8.3. Checking lock-wait-timeout with REVOKE ALL (table level).
########################################################################
CREATE TABLE t1(a INT);
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT ON t1 TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT user, table_name, table_priv FROM mysql.tables_priv WHERE host = 'h';
user	table_name	table_priv
DROP USER u1@h, u2@h, u3@h;
DROP TABLE t1;

########################################################################
# A.8.4. Checking lock-wait-timeout with REVOKE ALL (column level).
########################################################################
CREATE TABLE t1(a INT);
CREATE USER u1@h, u2@h, u3@h;
GRANT SELECT(a) ON t1 TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT user, column_name, column_priv FROM mysql.columns_priv WHERE host = 'h';
user	column_name	column_priv
DROP USER u1@h, u2@h, u3@h;
DROP TABLE t1;

########################################################################
# A.8.5. Checking lock-wait-timeout with REVOKE ALL (stored routine level).
########################################################################
CREATE PROCEDURE p1() SELECT 1;
CREATE USER u1@h, u2@h, u3@h;
GRANT EXECUTE ON PROCEDURE p1 TO u1@h, u2@h, u3@h;
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u1	h
u2	h
u3	h
START TRANSACTION;
SELECT user, host FROM mysql.user WHERE user = 'u2' AND host = 'h' FOR UPDATE;
user	host
u2	h

# Connection: con1
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h, u2@h, u3@h;

# Connection: default
ROLLBACK;

# Connection: con1

# Connection: default
SELECT user FROM mysql.user WHERE host = 'h';
user
u1
u2
u3
SELECT user, routine_name, proc_priv FROM mysql.procs_priv WHERE host = 'h';
user	routine_name	proc_priv
DROP USER u1@h, u2@h, u3@h;
DROP PROCEDURE p1;

########################################################################
# A.9. Checking lock-wait-timeout with FLUSH PRIVILEGES.
########################################################################
START TRANSACTION;
INSERT INTO mysql.user(user, host, ssl_cipher, x509_issuer, x509_subject)
VALUES ('u2', 'h', '', '', '');

# Connection: con1
FLUSH PRIVILEGES;;

# Connection: default
COMMIT;

# Connection: con1

# Connection: default
SELECT user, host FROM mysql.user WHERE host = 'h';
user	host
u2	h
DROP USER u2@h;

########################################################################
# B.1 Check that SET PASSWORD commits a transaction.
########################################################################
CREATE TABLE t1(a INT);
CREATE USER u1@h;
START TRANSACTION;
INSERT INTO t1 VALUES (1), (2), (3);

# Connection: con1
SELECT * FROM t1;
a
ROLLBACK;

# Connection: default
SET PASSWORD FOR u1@h = '';

# Connection: con1
SELECT * FROM t1;
a
1
2
3
ROLLBACK;

# Connection: default
DROP TABLE t1;

########################################################################
# B.2 Check that SET PASSWORD can't be called from a stored function
# directly or indirectly..
########################################################################
CREATE PROCEDURE p1() SET PASSWORD FOR u1@h = '12345';
CREATE FUNCTION f1() RETURNS INT
BEGIN
SET PASSWORD FOR u1@h = '12345';
RETURN 0;
END;
$
ERROR HY000: Not allowed to set autocommit from a stored function or trigger
CREATE FUNCTION f2()  RETURNS INT
BEGIN
CALL p1();
RETURN 0;
END;
$
SELECT f2();
ERROR HY000: Not allowed to set autocommit from a stored function or trigger
DROP FUNCTION f2;
DROP PROCEDURE p1;
DROP USER u1@h;

####################################################################
# C.1. Tests for 'DROP USER' statement
####################################################################
SELECT user, host FROM mysql.user where user like 'user%';
user	host
CREATE USER user2@localhost;
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user2	localhost
DROP USER user2@localhost;
SELECT user, host FROM mysql.user where user like 'user%';
user	host
DROP USER no_user@localhost;
ERROR HY000: Operation DROP USER failed for 'no_user'@'localhost'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation DROP USER failed for 'no_user'@'localhost'
CREATE USER user2@localhost, user3@localhost;
DROP USER no_user@localhost, user3@localhost,
no_user1@localhost, user2@localhost,
no_user2@localhost;
ERROR HY000: Operation DROP USER failed for 'no_user'@'localhost','no_user1'@'localhost','no_user2'@'localhost'
DROP USER user2@localhost, user3@localhost;
SELECT user, host FROM mysql.user where user like 'user%';
user	host

####################################################################
# C.2. Tests for 'RENAME USER' statement
####################################################################
SELECT user, host FROM mysql.user where user like 'user%';
user	host
CREATE USER user2@localhost;
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user2	localhost
RENAME USER user2@localhost TO user3@localhost;
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user3	localhost
RENAME USER no_user@localhost to user6@localhost;
ERROR HY000: Operation RENAME USER failed for 'no_user'@'localhost'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation RENAME USER failed for 'no_user'@'localhost'
RENAME USER user3@localhost TO user6@localhost,
no_user@localhost TO user3@localhost;
ERROR HY000: Operation RENAME USER failed for 'no_user'@'localhost'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation RENAME USER failed for 'no_user'@'localhost'
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user3	localhost
CREATE user user7@localhost;
RENAME USER user6@localhost TO temp_user@localhost,
user7@localhost TO user4@localhost,
no_user@localhost TO no_user1@localhost,
temp_user@localhost to user7@localhost;
ERROR HY000: Operation RENAME USER failed for 'user6'@'localhost','no_user'@'localhost','temp_user'@'localhost'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation RENAME USER failed for 'user6'@'localhost','no_user'@'localhost','temp_user'@'localhost'
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user3	localhost
user7	localhost
RENAME USER no_user@localhost TO user7@localhost,
user7@localhost TO user8@localhost,
no_user1@localhost TO user11@localhost,
user4@localhost TO user7@localhost,
no_user2@localhost TO user12@localhost;
ERROR HY000: Operation RENAME USER failed for 'no_user'@'localhost','no_user1'@'localhost','user4'@'localhost','no_user2'@'localhost'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation RENAME USER failed for 'no_user'@'localhost','no_user1'@'localhost','user4'@'localhost','no_user2'@'localhost'
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user3	localhost
user7	localhost
SET debug="+d,wl7158_handle_grant_table_1";
RENAME USER user7@localhost TO user7_1@localhost;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_handle_grant_table_1";
DROP USER user7@localhost;

####################################################################
# C.3. Tests for 'CREATE USER' statement
####################################################################
CREATE USER user2@localhost;
CREATE USER user2@localhost;
ERROR HY000: Operation CREATE USER failed for 'user2'@'localhost'
CREATE USER user3@localhost, user2@localhost, user4@localhost;
ERROR HY000: Operation CREATE USER failed for 'user3'@'localhost','user2'@'localhost'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation CREATE USER failed for 'user3'@'localhost','user2'@'localhost'
SELECT user, host FROM mysql.user where user like 'user%';
user	host
user2	localhost
user3	localhost
DROP USER user2@localhost, user3@localhost;

####################################################################
# C.4. Tests for 'ALTER USER' statement
####################################################################
CREATE USER u2@l;
ALTER USER u2@l PASSWORD EXPIRE INTERVAL 5 day;
SELECT user, password_lifetime FROM mysql.user where USER like 'u_';
user	password_lifetime
u2	5
ALTER USER u3@l PASSWORD EXPIRE INTERVAL 5 DAY;
ERROR HY000: Operation ALTER USER failed for 'u3'@'l'
ALTER USER u3@l, u2@l, u4@l PASSWORD EXPIRE INTERVAL 7 DAY;
ERROR HY000: Operation ALTER USER failed for 'u3'@'l','u4'@'l'
SHOW WARNINGS;
Level	Code	Message
Error	1396	Operation ALTER USER failed for 'u3'@'l','u4'@'l'
SELECT user, password_lifetime FROM mysql.user WHERE user LIKE 'u_';
user	password_lifetime
u2	5
DROP USER u2@l;

####################################################################
# C.5. Tests for 'GRANT' and 'REVOKE' statement
####################################################################
call mtr.add_suppression("Did not write failed .*");
call mtr.add_suppression("REVOKE/GRANT failed .* An incident event has been written to the binary log which will stop the slaves.");
CREATE TABLE t1(a INT, b INT);
CREATE USER u1@h, u3@h;
GRANT SELECT(a) ON t1 TO u1@h, u3@h;
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

# GRANT on non existing column for non existing user
# (the whole statement fails, no change will be made).
GRANT SELECT(b), SELECT(c), INSERT(b) ON t1 TO u1@h, u2@h, u3@h;
ERROR 42S22: Unknown column 'c' in 't1'
SHOW WARNINGS;
Level	Code	Message
Error	1054	Unknown column 'c' in 't1'
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

# GRANT for non existing user.
# (only non-existing user components fail).
GRANT SELECT(b) ON t1 TO u1@h, u2@h, u3@h;
ERROR 42000: You are not allowed to create a user with GRANT
SHOW WARNINGS;
Level	Code	Message
Error	1410	You are not allowed to create a user with GRANT
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

# GRANT on non existing column for u1@h;
GRANT SELECT(b), SELECT(c), INSERT(b) ON t1 TO u1@h;
ERROR 42S22: Unknown column 'c' in 't1'
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

# REVOKE with existing and non existing user (REVOKE removed privileges
# for existing accounts despite an error).
REVOKE SELECT(a) ON t1 FROM u0@h, u1@h, u2@h, u3@h;
ERROR 42000: There is no such grant defined for user 'u0' on host 'h' on table 't1'
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

GRANT SELECT(a) ON t1 TO u1@h;
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

# REVOKE with existing and non existing column
REVOKE SELECT(a), SELECT(b) ON t1 FROM u1@h;
ERROR 42000: There is no such grant defined for user 'u1' on host 'h' on table 't1'
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

GRANT SELECT(a) ON t1 TO u1@h;
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

# REVOKE with existing, non existing column / user.
REVOKE SELECT(a),SELECT(c) ON t1 FROM u1@h;
ERROR 42000: There is no such grant defined for user 'u1' on host 'h' on table 't1'
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
u1	a	Select
u3	a	Select

DROP USER u1@h, u3@h;
DROP TABLE t1;

# Check that the statement REVOKE handles correctly in the case
# when some of a user in the list for REVOKE is deleted manually
# before REVOKE has been run

CREATE USER u1@h;
CREATE USER u2@h;
CREATE SCHEMA test1;
CREATE TABLE test1.t1 (a INT);
GRANT SELECT ON test1.* TO u1@h;
GRANT SELECT ON test1.* TO u2@h;
DELETE FROM mysql.db WHERE host = 'h' AND user = 'u1';
COMMIT;
REVOKE SELECT ON test1.* FROM u1@h, u2@h;
ERROR 42000: There is no such grant defined for user 'u1' on host 'h'
DROP SCHEMA test1;
DROP USER u1@h, u2@h;

# Check that DROP PROCEDURE handles correctly in the case
# when grant information for some user was manually deleted
# from the mysql.procs_priv

CREATE USER u1@h;
CREATE USER u2@h;
CREATE PROCEDURE p1() SET @a :=1;
GRANT EXECUTE ON PROCEDURE p1 TO u1@h;
GRANT EXECUTE ON PROCEDURE p1 TO u2@h;
DELETE FROM mysql.procs_priv WHERE routine_name='p1';
COMMIT;
DROP PROCEDURE p1;
DROP USER u1@h, u2@h;

####################################################################
# D. Simulate the SE failure and check the ACL statement behavior.
####################################################################
CREATE TABLE t1(a INT);
CREATE USER user1@;
GRANT UPDATE (a) ON t1 TO user1@;
SET debug='+d,wl7158_grant_table_1';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug='-d,wl7158_grant_table_1';
SET debug='+d,wl7158_grant_table_2';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug='-d,wl7158_grant_table_2';
SET debug='+d,wl7158_grant_table_3';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug='-d,wl7158_grant_table_3';
DROP USER user1@;
DROP TABLE t1;
CREATE USER u1@h;
CREATE PROCEDURE p() SET @x = 1;
CREATE PROCEDURE p1() SET @y = 1;
GRANT EXECUTE ON PROCEDURE p TO u1@h;
GRANT EXECUTE ON PROCEDURE p1 TO u1@h;
SET debug='+d,wl7158_grant_load_1';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Error	1105	Unknown error
SET debug='-d,wl7158_grant_load_1';
SET debug='+d,wl7158_grant_load_proc_1';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Error	1105	Unknown error
SET debug='-d,wl7158_grant_load_proc_1';
SET debug='+d,wl7158_grant_load_2';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Error	1105	Unknown error
SET debug='-d,wl7158_grant_load_2';
SET debug="+d,wl7158_grant_load_proc_2";
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Error	1105	Unknown error
SET debug="-d,wl7158_grant_load_proc_2";
FLUSH PRIVILEGES;
GRANT EXECUTE on PROCEDURE p to u1@h;
SET debug="+d,wl7158_grant_load_proc_3";
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Error	1105	Unknown error
SET debug="-d,wl7158_grant_load_proc_3";
FLUSH PRIVILEGES;
CREATE TABLE t1(a INT);
GRANT SELECT ON t1 TO u1@h;
SET debug='+d,wl7158_grant_load_3';
FLUSH PRIVILEGES;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Error	1105	Unknown error
SET debug='-d,wl7158_grant_load_3';
DROP TABLE t1;
DROP USER u1@h;
DROP PROCEDURE p;
DROP PROCEDURE p1;
SET debug="+d,wl7158_replace_user_table_1";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
ERROR 42000: There is no such grant defined for user 'u1' on host 'h'
SET debug="-d,wl7158_replace_user_table_1";
CREATE USER u1@h;
SET debug="+d,wl7158_replace_user_table_1";
SET PASSWORD FOR u1@h = 'systpass';
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_user_table_1";
SET debug="+d,wl7158_replace_user_table_2";
SET PASSWORD FOR u1@h = 'systpass';
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_user_table_2";
DROP USER u1@h;
CREATE USER u1@h;
SET debug="+d,wl7158_replace_user_table_1";
ALTER USER u1@h REQUIRE CIPHER "EDH-RSA-DES-CBC3-SHA";
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_user_table_1";
ALTER USER u1@h REQUIRE CIPHER "EDH-RSA-DES-CBC3-SHA";
GRANT SELECT ON mysqltest.* TO u1@h;
SET debug="+d,wl7158_replace_user_table_2";
ALTER USER u1@h REQUIRE CIPHER "EDH-RSA-DES-CBC3-SHA"
  AND SUBJECT "testsubject" ISSUER "MySQL";
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT ON `mysqltest`.* TO `u1`@`h`
SET debug="-d,wl7158_replace_user_table_2";
DROP USER u1@h;
SET debug="+d,wl7158_replace_user_table_1";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_user_table_1";
CREATE USER u1@h;
CREATE TABLE t1(a INT);
SET debug="+d,mysql_table_grant_out_of_memory";
CALL mtr.add_suppression(".*Out of memory *");
GRANT SELECT ON t1 TO u1@h;
Got one of the listed errors
SET debug="-d,mysql_table_grant_out_of_memory";
DROP TABLE t1;
DROP USER u1@h;
SET debug="+d,wl7158_replace_user_table_3";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_user_table_3";
CREATE USER u1@h;
SET debug="+d,wl7158_replace_db_table_1";
GRANT SELECT ON mysqltest.* TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_db_table_1";
GRANT SELECT ON mysqltest.* TO u1@h;
SET debug="+d,wl7158_replace_db_table_2";
GRANT SELECT ON mysqltest.* TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT ON `mysqltest`.* TO `u1`@`h`
SET debug="-d,wl7158_replace_db_table_2";
SET debug="+d,wl7158_replace_db_table_3";
REVOKE ALL PRIVILEGES ON mysqltest.* FROM u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT ON `mysqltest`.* TO `u1`@`h`
SET debug="-d,wl7158_replace_db_table_3";
DROP USER u1@h;
CREATE USER u1@h;
SET debug="+d,wl7158_replace_db_table_4";
GRANT SELECT ON mysqltest.* TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_replace_db_table_4";
DROP USER u1@h;
SET debug="+d,mysql_handle_grant_data_fail_on_routine_table";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,mysql_handle_grant_data_fail_on_routine_table";
SET debug="+d,mysql_handle_grant_data_fail_on_tables_table";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,mysql_handle_grant_data_fail_on_tables_table";
SET debug="+d,mysql_handle_grant_data_fail_on_columns_table";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,mysql_handle_grant_data_fail_on_columns_table";
SET debug="+d,mysql_handle_grant_data_fail_on_proxies_priv_table";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,mysql_handle_grant_data_fail_on_proxies_priv_table";
SET debug="+d,wl7158_handle_grant_table_1";
CREATE USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
ERROR 42000: There is no such grant defined for user 'u1' on host 'h'
SET debug="-d,wl7158_handle_grant_table_1";
CREATE USER u1@h;
SET debug="+d,wl7158_modify_grant_table_1";
rename user u1@h to u2@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u2@h;
ERROR 42000: There is no such grant defined for user 'u2' on host 'h'
SHOW WARNINGS;
Level	Code	Message
Error	1141	There is no such grant defined for user 'u2' on host 'h'
SET debug="-d,wl7158_modify_grant_table_1";
DROP USER u1@h;
CREATE USER u1@h;
SET debug="+d,wl7158_modify_grant_table_2";
DROP USER u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SET debug="-d,wl7158_modify_grant_table_2";
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
DROP USER u1@h;
CREATE USER u1@h;
SET debug="+d,wl7158_replace_proxies_priv_table_1";
GRANT PROXY ON plug_dest TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_proxies_priv_table_1";
SET debug="+d,wl7158_replace_proxies_priv_table_2";
GRANT PROXY ON plug_dest TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_proxies_priv_table_2";
SET debug="+d,wl7158_replace_proxies_priv_table_5";
GRANT PROXY ON plug_dest TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_proxies_priv_table_5";
GRANT PROXY ON plug_dest TO u1@h;
SET debug="+d,wl7158_replace_proxies_priv_table_3";
GRANT PROXY ON plug_dest TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT PROXY ON `plug_dest`@`%` TO `u1`@`h`
SET debug="-d,wl7158_replace_proxies_priv_table_3";
SET debug="+d,wl7158_replace_proxies_priv_table_4";
REVOKE PROXY ON plug_dest FROM u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT PROXY ON `plug_dest`@`%` TO `u1`@`h`
SET debug="-d,wl7158_replace_proxies_priv_table_4";
DROP USER u1@h;
CREATE TABLE t1 (a INT);
CREATE USER u1@h;
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="+d,wl7158_replace_column_table_1";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_1";
SET debug="+d,wl7158_replace_column_table_2";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_2";
SET debug="+d,wl7158_replace_column_table_5";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_5";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
SET debug="+d,wl7158_replace_column_table_3";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT (`a`), INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t1` TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_3";
SET debug="+d,wl7158_replace_column_table_4";
REVOKE SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 FROM u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT (`a`), INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t1` TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_4";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
SET debug="+d,wl7158_replace_column_table_6";
REVOKE SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 FROM u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT (`a`), INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t1` TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_6";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
SET debug="+d,wl7158_replace_column_table_7";
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT (`a`), INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t1` TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_7";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
SET debug="+d,wl7158_replace_column_table_8";
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT SELECT (`a`), INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t1` TO `u1`@`h`
SET debug="-d,wl7158_replace_column_table_8";
CREATE TABLE t2 (a INT);
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
GRANT INSERT(a) ON t2 TO u1@h;
SELECT host, db, user, table_name, column_name, column_priv FROM mysql.columns_priv;
host	db	user	table_name	column_name	column_priv
h	test	u1	t1	a	Select,Insert,Update,References
h	test	u1	t2	a	Insert
SELECT host, db, user, table_name, grantor, table_priv, column_priv FROM mysql.tables_priv;
host	db	user	table_name	grantor	table_priv	column_priv
h	test	u1	t1	root@localhost		Select,Insert,Update,References
h	test	u1	t2	root@localhost		Insert
localhost	mysql	mysql.session	user	root@localhost	Select	
localhost	sys	mysql.sys	sys_config	root@localhost	Select	
DELETE FROM mysql.columns_priv WHERE host = 'h' AND user = 'u1'
AND table_name = 't1';
COMMIT;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h;
SELECT host, db, user, table_name, column_name, column_priv FROM mysql.columns_priv;
host	db	user	table_name	column_name	column_priv
SELECT host, db, user, table_name, grantor, table_priv, column_priv FROM mysql.tables_priv;
host	db	user	table_name	grantor	table_priv	column_priv
localhost	mysql	mysql.session	user	root@localhost	Select	
localhost	sys	mysql.sys	sys_config	root@localhost	Select	
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT UPDATE (a) ON t1 TO u1@h;
SET debug='+d,mysql_grant_table_init_out_of_memory';
FLUSH PRIVILEGES;
Got one of the listed errors
SET debug='-d,mysql_grant_table_init_out_of_memory';
DROP TABLE t1;
DROP TABLE t2;
DROP USER u1@h;
# Test case that checks the whole transaction was rolled back
# in the case of system error and that in-memory cache was reloaded after that
CREATE USER u1@h;
CREATE TABLE t1 (a INT);
SET debug="+d,wl7158_replace_column_table_5";
GRANT SELECT(a), UPDATE(a), INSERT(a), REFERENCES(a) ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
# Check that changes of the whole statement have been rolled back.
SELECT * FROM mysql.columns_priv WHERE host = 'h' AND user = 'u1';
Host	Db	User	Table_name	Column_name	Timestamp	Column_priv
SELECT * FROM mysql.tables_priv WHERE host = 'h' AND user = 'u1';
Host	Db	User	Table_name	Grantor	Timestamp	Table_priv	Column_priv
SET debug="-d,wl7158_replace_column_table_5";
DROP TABLE t1;
DROP USER u1@h;
CREATE TABLE t1 (a INT);
CREATE USER u1@h;
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="+d,wl7158_replace_table_table_1";
GRANT UPDATE ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_table_table_1";
SET debug="+d,wl7158_replace_table_table_3";
GRANT UPDATE ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_table_table_3";
GRANT UPDATE ON t1 TO u1@h;
SET debug="+d,wl7158_replace_table_table_2";
GRANT UPDATE ON t1 TO u1@h;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT UPDATE ON `test`.`t1` TO `u1`@`h`
SET debug="-d,wl7158_replace_table_table_2";
DROP TABLE t1;
DROP USER u1@h;
CREATE TABLE t1 (a INT);
CREATE USER u1@h;
CREATE PROCEDURE p() SET @x = 1;
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="+d,wl7158_replace_routine_table_1";
GRANT USAGE ON PROCEDURE p TO u1@h WITH GRANT OPTION;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_routine_table_1";
SET debug="+d,wl7158_replace_routine_table_4";
GRANT USAGE ON PROCEDURE p TO u1@h WITH GRANT OPTION;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
SET debug="-d,wl7158_replace_routine_table_4";
GRANT USAGE ON PROCEDURE p TO u1@h WITH GRANT OPTION;
SET debug="+d,wl7158_replace_routine_table_2";
GRANT USAGE ON PROCEDURE p TO u1@h WITH GRANT OPTION;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT USAGE ON PROCEDURE `test`.`p` TO `u1`@`h` WITH GRANT OPTION
SET debug="-d,wl7158_replace_routine_table_2";
SET debug="+d,wl7158_replace_routine_table_3";
DROP PROCEDURE p;
ERROR HY000: The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
SHOW WARNINGS;
Level	Code	Message
Error	3501	The ACL operation failed due to the following error from SE: errcode 149 - Lock deadlock; Retry transaction
Warning	1405	Failed to revoke all privileges to dropped routine
SHOW GRANTS FOR u1@h;
Grants for u1@h
GRANT USAGE ON *.* TO `u1`@`h`
GRANT USAGE ON PROCEDURE `test`.`p` TO `u1`@`h` WITH GRANT OPTION
SET debug="-d,wl7158_replace_routine_table_3";
DROP TABLE t1;
DROP USER u1@h;
########################################################################
# Checking that it is possible to CREATE privilege tables in MyISAM.
# This is necessary for upgrade via mysqldump to work.
########################################################################
RENAME TABLE mysql.user TO mysql.user_bak;
RENAME TABLE mysql.db TO mysql.db_bak;
RENAME TABLE mysql.columns_priv TO mysql.columns_priv_bak;
RENAME TABLE mysql.procs_priv TO mysql.procs_priv_bak;
RENAME TABLE mysql.proxies_priv TO mysql.proxies_priv_bak;
RENAME TABLE mysql.tables_priv TO mysql.tables_priv_bak;
CREATE TABLE mysql.db ENGINE=MyISAM SELECT * FROM mysql.db_bak;
Warnings:
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.db]
CREATE TABLE mysql.user ENGINE=MyISAM SELECT * FROM mysql.user_bak;
Warnings:
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.user]
CREATE TABLE mysql.columns_priv ENGINE=MyISAM SELECT * FROM mysql.columns_priv_bak;
Warnings:
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.columns_priv]
CREATE TABLE mysql.procs_priv ENGINE=MyISAM SELECT * FROM mysql.procs_priv_bak;
Warnings:
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.procs_priv]
CREATE TABLE mysql.proxies_priv ENGINE=MyISAM SELECT * FROM mysql.proxies_priv_bak;
Warnings:
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.proxies_priv]
CREATE TABLE mysql.tables_priv ENGINE=MyISAM SELECT * FROM mysql.tables_priv_bak;
Warnings:
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.tables_priv]
DROP TABLE mysql.user, mysql.db, mysql.columns_priv,
mysql.procs_priv, mysql.proxies_priv, mysql.tables_priv;
RENAME TABLE mysql.user_bak TO mysql.user;
RENAME TABLE mysql.db_bak TO mysql.db;
RENAME TABLE mysql.columns_priv_bak TO mysql.columns_priv;
RENAME TABLE mysql.procs_priv_bak TO mysql.procs_priv;
RENAME TABLE mysql.proxies_priv_bak TO mysql.proxies_priv;
RENAME TABLE mysql.tables_priv_bak TO mysql.tables_priv;

########################################################################
# F. Upgrade/downgrade tests.
########################################################################

# Disconnect con1

# Connection: default
CREATE USER u1@h;

# Remove the session user
DELETE FROM mysql.tables_priv WHERE user='mysql.session';
DELETE FROM mysql.user WHERE user='mysql.session';
DELETE FROM mysql.db WHERE user='mysql.session';

# Backup privilege tables and drop original ones
RENAME TABLE mysql.user TO mysql.user_bak;
RENAME TABLE mysql.db TO mysql.db_bak;
RENAME TABLE mysql.columns_priv TO mysql.columns_priv_bak;
RENAME TABLE mysql.procs_priv TO mysql.procs_priv_bak;
RENAME TABLE mysql.proxies_priv TO mysql.proxies_priv_bak;
RENAME TABLE mysql.tables_priv TO mysql.tables_priv_bak;
# Create exact structure of privilege tables as it is in 5.6
CREATE TABLE IF NOT EXISTS mysql.db (   Host char(60) binary DEFAULT '' NOT NULL,
Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL,
Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
PRIMARY KEY Host (Host,Db,User), KEY User (User) ) engine=MyISAM
CHARACTER SET utf8 COLLATE utf8_bin comment='Database privileges';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.db]
CREATE TABLE IF NOT EXISTS mysql.user (   Host char(60) binary DEFAULT '' NOT NULL,
User char(16) binary DEFAULT '' NOT NULL, Password char(41) character set latin1
collate latin1_bin DEFAULT '' NOT NULL, Select_priv enum('N','Y') COLLATE utf8_general_ci
DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Reload_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Shutdown_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Process_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
File_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_tablespace_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci DEFAULT '' NOT NULL,
ssl_cipher BLOB NOT NULL, x509_issuer BLOB NOT NULL, x509_subject BLOB NOT NULL,
max_questions int(11) unsigned DEFAULT 0  NOT NULL,
max_updates int(11) unsigned DEFAULT 0  NOT NULL,
max_connections int(11) unsigned DEFAULT 0  NOT NULL,
max_user_connections int(11) unsigned DEFAULT 0  NOT NULL,
plugin char(64) DEFAULT 'caching_sha2_password', authentication_string TEXT,
password_expired ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
PRIMARY KEY Host (Host,User) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin
comment='Users and global privileges';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.user]
CREATE TABLE IF NOT EXISTS mysql.tables_priv ( Host char(60) binary DEFAULT '' NOT NULL,
Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL,
Table_name char(64) binary DEFAULT '' NOT NULL, Grantor char(77) DEFAULT '' NOT NULL,
Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References',
'Index','Alter','Create View','Show view','Trigger') COLLATE utf8_general_ci DEFAULT '' NOT NULL,
Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT ''
 NOT NULL, PRIMARY KEY (Host,Db,User,Table_name), KEY Grantor (Grantor) )
engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Table privileges';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.tables_priv]
CREATE TABLE IF NOT EXISTS mysql.columns_priv ( Host char(60) binary DEFAULT '' NOT NULL,
Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL,
Table_name char(64) binary DEFAULT '' NOT NULL, Column_name char(64) binary DEFAULT '' NOT NULL,
Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci
DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name,Column_name) )
engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Column privileges';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.columns_priv]
CREATE TABLE IF NOT EXISTS mysql.procs_priv ( Host char(60) binary DEFAULT '' NOT NULL,
Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL,
Routine_name char(64) COLLATE utf8_general_ci DEFAULT '' NOT NULL,
Routine_type enum('FUNCTION','PROCEDURE') NOT NULL, Grantor char(77) DEFAULT '' NOT NULL,
Proc_priv set('Execute','Alter Routine','Grant') COLLATE utf8_general_ci DEFAULT '' NOT NULL,
Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
PRIMARY KEY (Host,Db,User,Routine_name,Routine_type), KEY Grantor (Grantor) )
engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Procedure privileges';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.procs_priv]
CREATE TABLE IF NOT EXISTS mysql.proxies_priv (Host char(60) binary DEFAULT '' NOT NULL,
User char(16) binary DEFAULT '' NOT NULL, Proxied_host char(60) binary DEFAULT '' NOT NULL,
Proxied_user char(16) binary DEFAULT '' NOT NULL, With_grant BOOL DEFAULT 0 NOT NULL,
Grantor char(77) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY Host (Host,User,Proxied_host,Proxied_user),
KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin
comment='User proxy privileges';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1726	Storage engine 'MyISAM' does not support system tables. [mysql.proxies_priv]
# Restore original content of privilege tables
INSERT INTO mysql.user(Host, User, Select_priv, Insert_priv, Update_priv, Delete_priv,
Create_priv, Drop_priv, Reload_priv, Shutdown_priv, Process_priv, File_priv, Grant_priv,
References_priv, Index_priv, Alter_priv, Show_db_priv, Super_priv, Create_tmp_table_priv,
Lock_tables_priv, Execute_priv, Repl_slave_priv, Repl_client_priv, Create_view_priv,
Show_view_priv, Create_routine_priv, Alter_routine_priv, Create_user_priv, Event_priv,
Trigger_priv, Create_tablespace_priv, ssl_type, ssl_cipher, x509_issuer, x509_subject,
max_questions, max_updates, max_connections, max_user_connections, plugin,
authentication_string, password_expired) SELECT Host, User, Select_priv, Insert_priv,
Update_priv, Delete_priv, Create_priv, Drop_priv, Reload_priv, Shutdown_priv, Process_priv,
File_priv, Grant_priv, References_priv, Index_priv, Alter_priv, Show_db_priv, Super_priv,
Create_tmp_table_priv, Lock_tables_priv, Execute_priv, Repl_slave_priv, Repl_client_priv,
Create_view_priv, Show_view_priv, Create_routine_priv, Alter_routine_priv, Create_user_priv,
Event_priv, Trigger_priv, Create_tablespace_priv, ssl_type, ssl_cipher, x509_issuer,
x509_subject, max_questions, max_updates, max_connections, max_user_connections, plugin,
authentication_string, password_expired FROM mysql.user_bak;
INSERT INTO mysql.db(Host, Db, User, Select_priv, Insert_priv, Update_priv, Delete_priv,
Create_priv, Drop_priv, Grant_priv, References_priv, Index_priv, Alter_priv,
Create_tmp_table_priv, Lock_tables_priv, Create_view_priv, Show_view_priv, Create_routine_priv,
Alter_routine_priv, Execute_priv, Event_priv, Trigger_priv) SELECT Host, Db, User, Select_priv,
Insert_priv, Update_priv, Delete_priv, Create_priv, Drop_priv, Grant_priv, References_priv,
Index_priv, Alter_priv, Create_tmp_table_priv, Lock_tables_priv, Create_view_priv,
Show_view_priv, Create_routine_priv, Alter_routine_priv, Execute_priv, Event_priv,
Trigger_priv FROM mysql.db_bak;
INSERT INTO mysql.columns_priv(Host, Db, User, Table_name, Column_name, Timestamp, Column_priv)
SELECT Host, Db, User, Table_name, Column_name, Timestamp, Column_priv
FROM mysql.columns_priv_bak;
INSERT INTO mysql.procs_priv(Host, Db, User, Routine_name, Routine_type, Grantor, Proc_priv,
Timestamp) SELECT Host, Db, User, Routine_name, Routine_type, Grantor, Proc_priv, Timestamp
FROM mysql.procs_priv_bak;
INSERT INTO mysql.proxies_priv(Host, User, Proxied_host, Proxied_user, With_grant, Grantor,
Timestamp) SELECT Host, User, Proxied_host, Proxied_user, With_grant, Grantor, Timestamp
FROM mysql.proxies_priv_bak;
INSERT INTO mysql.tables_priv(Host, Db, User, Table_name, Grantor, Timestamp, Table_priv,
Column_priv) SELECT Host, Db, User, Table_name, Grantor, Timestamp, Table_priv, Column_priv
FROM mysql.tables_priv_bak;
COMMIT;
CREATE TABLE t1 (a INT);
call mtr.add_suppression("mysql.user has no `Event_priv` column at position 28");

# Shutting down mysqld...

# Starting mysqld in the regular mode...

# restart

# Check that any ACL statement is failed when privilege tables
# have engine type MyISAM

CREATE USER u2@h;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
ALTER USER u1@h PASSWORD EXPIRE;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
GRANT SELECT ON t1 TO u1@h;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
REVOKE SELECT ON t1 FROM u1@h;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
REVOKE ALL PRIVILEGES, GRANT OPTION FROM u1@h;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
SET PASSWORD FOR u1@h = '123';
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
RENAME USER u1@h TO u1_1@h;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
DROP USER u1@h;
ERROR HY000: Storage engine 'MyISAM' does not support system tables. [mysql.user]
SELECT table_name, engine FROM INFORMATION_SCHEMA.TABLES
WHERE table_schema = 'mysql' AND
table_name IN ('user', 'db', 'columns_priv', 'procs_priv',
'proxies_priv', 'tables_priv')
ORDER BY table_name;
TABLE_NAME	ENGINE
columns_priv	MyISAM
db	MyISAM
procs_priv	MyISAM
proxies_priv	MyISAM
tables_priv	MyISAM
user	MyISAM

# mysql_upgrade...
# restart:--upgrade=FORCE

# The ACL tables must have been upgraded.
SELECT table_name, engine FROM INFORMATION_SCHEMA.TABLES
WHERE table_schema = 'mysql' AND
table_name IN ('user', 'db', 'columns_priv', 'procs_priv',
'proxies_priv', 'tables_priv')
ORDER BY table_name;
TABLE_NAME	ENGINE
columns_priv	InnoDB
db	InnoDB
procs_priv	InnoDB
proxies_priv	InnoDB
tables_priv	InnoDB
user	InnoDB
# 
# Clean-up. Restore original state of privilege tables.
DROP TABLES mysql.user, mysql.db, mysql.columns_priv, mysql.procs_priv,
mysql.proxies_priv, mysql.tables_priv;
RENAME TABLE mysql.user_bak TO mysql.user;
RENAME TABLE mysql.db_bak TO mysql.db;
RENAME TABLE mysql.columns_priv_bak TO mysql.columns_priv;
RENAME TABLE mysql.procs_priv_bak TO mysql.procs_priv;
RENAME TABLE mysql.proxies_priv_bak TO mysql.proxies_priv;
RENAME TABLE mysql.tables_priv_bak TO mysql.tables_priv;
DROP USER 'mysql.session'@localhost;
CREATE USER 'mysql.session'@localhost IDENTIFIED WITH caching_sha2_password AS '$A$005$THISISACOMBINATIONOFINVALIDSALTANDPASSWORDTHATMUSTNEVERBRBEUSED' ACCOUNT LOCK;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'mysql.session'@localhost;
GRANT SELECT ON mysql.user TO 'mysql.session'@localhost;
GRANT SELECT ON `performance_schema`.* TO 'mysql.session'@localhost;
GRANT SUPER ON *.* TO 'mysql.session'@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO 'mysql.session'@localhost;
GRANT SESSION_VARIABLES_ADMIN ON *.* TO 'mysql.session'@localhost;
GRANT PERSIST_RO_VARIABLES_ADMIN ON *.* TO 'mysql.session'@localhost;
GRANT CLONE_ADMIN ON *.* TO 'mysql.session'@localhost;
GRANT BACKUP_ADMIN ON *.* TO 'mysql.session'@localhost;
GRANT SHUTDOWN ON *.* TO 'mysql.session'@localhost;
GRANT CONNECTION_ADMIN ON *.* TO 'mysql.session'@localhost;
GRANT SYSTEM_USER ON *.* TO 'mysql.session'@localhost;
FLUSH PRIVILEGES;
DROP USER u1@h;
DROP TABLE t1;
#
# Bug #21749169 -- ASSERTS 'ERROR != 146' PROVIDED BY WL#7158 AT VARIOUS PLACES ARE HIT
#
CREATE TABLE t1 (a INT);
CREATE USER u1;
GRANT ALL ON test.t1 TO 'u1' ;
SET autocommit = 0;
connection con1
SET autocommit = 0;
START TRANSACTION WITH CONSISTENT SNAPSHOT;
SAVEPOINT A;
DELETE FROM mysql . tables_priv WHERE user = 'u1' LIMIT 1;
ROLLBACK WORK TO SAVEPOINT A;
connection default
SET innodb_lock_wait_timeout= 1;
GRANT ALL ON t1 TO 'u1';
DROP USER u1;
DROP TABLE t1;
