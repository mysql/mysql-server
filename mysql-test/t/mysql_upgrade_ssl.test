
# mysql_upgrade tests requiring SSL support

--source include/not_valgrind.inc
--source include/mysql_upgrade_preparation.inc

call mtr.add_suppression("Column count of mysql.* is wrong. "
                         "Expected .*, found .*. "
                         "The table is probably corrupted");

--echo #
--echo # WL #8350 ENSURE 5.7 SUPPORTS SMOOTH LIVE UPGRADE FROM 5.6
--echo #

# Backup mysql.user table
DROP TABLE IF EXISTS tmp_user;
CREATE TABLE tmp_user AS (SELECT * FROM mysql.user);

# Create 5.6 mysql.user table layout

--source include/user_80_to_57.inc
--source include/user_57_to_56.inc

--echo # "Manualy" create user with sha256 password
INSERT INTO mysql.user VALUES 
('%','user_sha_pass_wp','','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','','','','',0,0,0,0,'sha256_password','$5$J=M`}N+i=%1o6z\'$Ns0lpHRzOCs9T4n5df6ZxAYsUaK1yFMnRGlp3T48AW/','N');

FLUSH PRIVILEGES;

--echo #"Manualy grant" super user privileges to user_sha_pass_wp
UPDATE mysql.user SET Select_priv='Y', Insert_priv='Y', Update_priv='Y', Delete_priv='Y', Create_priv='Y', Drop_priv='Y', Reload_priv='Y', Shutdown_priv='Y', Process_priv='Y', File_priv='Y', Grant_priv='Y', References_priv='Y', Index_priv='Y', Alter_priv='Y', Show_db_priv='Y', Super_priv='Y', Create_tmp_table_priv='Y', Lock_tables_priv='Y', Execute_priv='Y', Repl_slave_priv='Y', Repl_client_priv='Y', Create_view_priv='Y', Show_view_priv='Y', Create_routine_priv='Y', Alter_routine_priv='Y', Create_user_priv='Y', Event_priv='Y', Trigger_priv='Y', Create_tablespace_priv='Y' where user="user_sha_pass_wp";

FLUSH PRIVILEGES;

--echo #Run mysql_upgrade with user_sha_pass_wp -n i.e. user with sha256
--echo #password set. After mysql_upgrade finishes the mysql.user table should
--echo #have 5.7 layout thus no need to restore the dropped columns from
--echo #the begining of the test
--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc

DROP USER 'user_sha_pass_wp'@'%';
#
# Restore mysql.user content
TRUNCATE TABLE mysql.user;
INSERT INTO mysql.user SELECT * FROM tmp_user;
FLUSH PRIVILEGES;
DROP TABLE tmp_user;

--source include/mysql_upgrade_cleanup.inc
