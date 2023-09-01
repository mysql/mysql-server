#
--source include/resource_group_init.inc

--echo #
--echo # Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT INSENSITIVE NAMES.
--echo #                (This issue is observed with Resource groups too.)
--echo #
SET NAMES utf8mb3;

--echo #
--echo # Test case to verify Resource group with case and accent insensitive names.
--echo #
--disable_warnings
CREATE RESOURCE GROUP café TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
--enable_warnings

--echo # Resource group names are case and accent insensitive. So from the
--echo # data-dictionary "cafe" is obtained for the following statement. Since
--echo # MDL key comparison is case and accent sensitive, assert condition to verify
--echo # expected lock with name "test.cafe" fails (lock is obtained on the
--echo # test.café).
--error ER_RESOURCE_GROUP_EXISTS
CREATE RESOURCE GROUP cafe TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;

--echo # Following statement is to verify operation with the upper case name.
--error ER_RESOURCE_GROUP_EXISTS
CREATE RESOURCE GROUP CAFE TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
DROP RESOURCE GROUP CaFé;

SET NAMES default;
