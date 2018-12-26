RESET MASTER;
call mtr.add_suppression("REVOKE/GRANT failed while storing table level and column level grants in the privilege tables. An incident event has been written to the binary log which will stop the slaves.");
CREATE TABLE t1(a INT, b INT);
CREATE USER u1@h;
#
# Verify that u1 is added into mysql.user.
#
SELECT user FROM mysql.user where user='u1';
user
u1
GRANT SELECT(a) ON t1 TO u1@h, u2@h;
ERROR 42000: You are not allowed to create a user with GRANT
#
# u1@h should not have been granted any permission
#
SELECT user, column_name, column_priv FROM mysql.columns_priv;
user	column_name	column_priv
#
# Verify that the partially failed GRANT statement does not generate
# a gtid and save the gtid into @@GLOBAL.GTID_EXECUTED.
#
include/assert.inc [Does not generate MASTER_UUID:4 and commit it into @@GLOBAL.GTID_EXECUTED]
#
# Verify that the partially failed GRANT statement is not binlogged.
#
include/show_binlog_events.inc
DROP USER u1@h;
DROP TABLE t1;
