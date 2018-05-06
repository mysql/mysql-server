include/rpl_init.inc [topology=1->2, 2->3]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection server_1]
CREATE TABLE t1 (a INT);
include/rpl_sync.inc
cast('XX:YY:ZZ' as time) >= cast('XX:YY:ZZ' as time))
1
# 1. Save both timestamps (original/immediate) on all servers.
[connection server_1]
[connection server_2]
[connection server_3]
# 2. Verify that the timestamps are correct.
include/assert.inc [Assert that the OCT is the same on server_1 and server_2.]
include/assert.inc [Assert that the OCT is the same on server_2 and server_3.]
include/assert.inc [Assert that OCT is the same as ICT on server_1]
include/assert.inc [Assert that ICT > OCT on server_2.]
include/assert.inc [Assert that ICT > OCT on server_3.]
include/assert.inc [Assert that ICT on server_1 < ICT on server_2.]
include/assert.inc [Assert that ICT on server_2 < ICT on server_3.]
[connection server_1]
INSERT INTO t1 VALUES(1);
include/rpl_sync.inc
cast('XX:YY:ZZ' as time) >= cast('XX:YY:ZZ' as time))
1
# 1. Save both timestamps (original/immediate) on all servers.
[connection server_1]
[connection server_2]
[connection server_3]
# 2. Verify that the timestamps are correct.
include/assert.inc [Assert that the OCT is the same on server_1 and server_2.]
include/assert.inc [Assert that the OCT is the same on server_2 and server_3.]
include/assert.inc [Assert that OCT is the same as ICT on server_1]
include/assert.inc [Assert that ICT > OCT on server_2.]
include/assert.inc [Assert that ICT > OCT on server_3.]
include/assert.inc [Assert that ICT on server_1 < ICT on server_2.]
include/assert.inc [Assert that ICT on server_2 < ICT on server_3.]
[connection server_1]
UPDATE t1 SET a=2;
include/rpl_sync.inc
cast('XX:YY:ZZ' as time) >= cast('XX:YY:ZZ' as time))
1
# 1. Save both timestamps (original/immediate) on all servers.
[connection server_1]
[connection server_2]
[connection server_3]
# 2. Verify that the timestamps are correct.
include/assert.inc [Assert that the OCT is the same on server_1 and server_2.]
include/assert.inc [Assert that the OCT is the same on server_2 and server_3.]
include/assert.inc [Assert that OCT is the same as ICT on server_1]
include/assert.inc [Assert that ICT > OCT on server_2.]
include/assert.inc [Assert that ICT > OCT on server_3.]
include/assert.inc [Assert that ICT on server_1 < ICT on server_2.]
include/assert.inc [Assert that ICT on server_2 < ICT on server_3.]
[connection server_1]
DELETE FROM t1 WHERE a=2;
include/rpl_sync.inc
cast('XX:YY:ZZ' as time) >= cast('XX:YY:ZZ' as time))
1
# 1. Save both timestamps (original/immediate) on all servers.
[connection server_1]
[connection server_2]
[connection server_3]
# 2. Verify that the timestamps are correct.
include/assert.inc [Assert that the OCT is the same on server_1 and server_2.]
include/assert.inc [Assert that the OCT is the same on server_2 and server_3.]
include/assert.inc [Assert that OCT is the same as ICT on server_1]
include/assert.inc [Assert that ICT > OCT on server_2.]
include/assert.inc [Assert that ICT > OCT on server_3.]
include/assert.inc [Assert that ICT on server_1 < ICT on server_2.]
include/assert.inc [Assert that ICT on server_2 < ICT on server_3.]
[connection server_1]
DROP TABLE t1;
include/rpl_sync.inc
cast('XX:YY:ZZ' as time) >= cast('XX:YY:ZZ' as time))
1
# 1. Save both timestamps (original/immediate) on all servers.
[connection server_1]
[connection server_2]
[connection server_3]
# 2. Verify that the timestamps are correct.
include/assert.inc [Assert that the OCT is the same on server_1 and server_2.]
include/assert.inc [Assert that the OCT is the same on server_2 and server_3.]
include/assert.inc [Assert that OCT is the same as ICT on server_1]
include/assert.inc [Assert that ICT > OCT on server_2.]
include/assert.inc [Assert that ICT > OCT on server_3.]
include/assert.inc [Assert that ICT on server_1 < ICT on server_2.]
include/assert.inc [Assert that ICT on server_2 < ICT on server_3.]
# 2. Verify that the Original_commit_timestamps are correct.
include/assert.inc [Assert that the OCT of a transaction is higher than the earlier transaction..]
include/assert.inc [Assert that the OCT of a transaction is higher than  the earlier transaction..]
include/assert.inc [Assert that the OCT of a transaction is higher than the earlier transaction..]
include/assert.inc [Assert that the OCT of a transaction is higher than the earlier transaction..]
include/rpl_end.inc
