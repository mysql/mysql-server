--echo # Create Resource Group Tests

--echo # 1. Invalid Resource Group Name.
--error 1064
CREATE RESOURCE GROUP 'invalid_utf8¿ÿres' TYPE=USER VCPU=1-3, 1;

--echo # 2. Invalid Resource Group Type
--error 1064
CREATE RESOURCE GROUP test TYPE=INVALID_TYPE VCPU=3-1, 0;

--echo # 3. Invalid VCPU ID and VCPU Range
--replace_result $num_vcpu MAX_CPU_NUM
--error ER_INVALID_VCPU_ID
--eval CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=$num_vcpu

--echo # 3.1 Invalid VCPU Range.
--error ER_INVALID_VCPU_RANGE
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=3-1;

--echo # 4. Invalid Value of thread priority.
if ($thr_prio_enable == 1)
{
--error ER_INVALID_THREAD_PRIORITY
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=25;

--echo # Create system resource group with invalid thread priority value.
--error ER_INVALID_THREAD_PRIORITY
CREATE RESOURCE GROUP system_resource TYPE=system THREAD_PRIORITY=25;
}

--echo # 5. Invalid Value of thread priority for user resource group type.
if ($thr_prio_enable == 1)
{
--error ER_INVALID_THREAD_PRIORITY
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=-5;
}

--echo # 5.1 Thread priority ignored warning.
if ($thr_prio_enable == 0)
{
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=-5;
SHOW WARNINGS;
}

--echo # 6. Create a valid user resource group.
if ($thr_prio_enable == 1)
{
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
}
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="user_resource";

--echo # 7. Resource group already exists error.
--error ER_RESOURCE_GROUP_EXISTS
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;

--echo # 7.1 Case sensitivity of RESOURCE GROUP.
--error ER_RESOURCE_GROUP_EXISTS
CREATE RESOURCE GROUP USER_resource TYPE=USER;
DROP RESOURCE GROUP user_resource;

--echo # 8. Privilege Error - user with default privileges can't create resource group.
CREATE USER u1;
--connect(con1,localhost,u1,'',)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;


--echo # 9. Privilege Error  - user with  RESOURCE_GROUP_USER privilege can't create resource group.
--connection default
GRANT RESOURCE_GROUP_USER ON *.* TO u1;
--connection con1
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
--connection default
--disconnect con1
DROP USER u1;

--echo # 10. Length of resource group name is greater than maximum allowed limit of identifier names.
--error ER_TOO_LONG_IDENT
CREATE RESOURCE GROUP user_resource_1234567890aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaafffffffff TYPE=USER;

--echo # ALTER resource group tests

--echo # 1. Alter on non-existent resource group.
--error ER_RESOURCE_GROUP_NOT_EXISTS
ALTER RESOURCE GROUP user_resource VCPU=3;

--echo # 2. Alter with a invalid CPU ID
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
--replace_result $num_vcpu MAX_CPU_NUM
--error ER_INVALID_VCPU_ID
--eval ALTER RESOURCE GROUP user_resource VCPU=$num_vcpu

--echo # 3. Alter with invalid CPU range.
--error ER_INVALID_VCPU_RANGE
ALTER RESOURCE GROUP user_resource VCPU=3-1;

--echo # 4. Alter with invalid thread_priority
if ($thr_prio_enable == 1)
{
--error ER_INVALID_THREAD_PRIORITY
ALTER RESOURCE GROUP user_resource THREAD_PRIORITY=25;
}

--echo # 5. Alter with invalid thread priority for a user resource group.
if ($thr_prio_enable == 1)
{
--error ER_INVALID_THREAD_PRIORITY
ALTER RESOURCE GROUP user_resource THREAD_PRIORITY=-5;
}
if ($thr_prio_enable == 0)
{
ALTER RESOURCE GROUP user_resource THREAD_PRIORITY=-5;
}

--echo # 6. DISABLE and ENABLE the resource group.
ALTER RESOURCE GROUP user_resource DISABLE;
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="user_resource";
ALTER RESOURCE GROUP user_resource ENABLE;

--echo # 7. Test FORCE option of ALTER
SET RESOURCE GROUP user_resource;
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP="user_resource";
--error ER_INVALID_USE_OF_FORCE_OPTION
ALTER RESOURCE GROUP user_resource FORCE;
ALTER RESOURCE GROUP user_resource DISABLE;
ALTER RESOURCE GROUP user_resource DISABLE FORCE;
SELECT COUNT(*) = 0 FROM performance_schema.threads WHERE RESOURCE_GROUP="user_resource";

--echo # 8. Privilege Error - user with default privileges can't alter resource group.
CREATE USER u1;
--connect(con1,localhost,u1,'',)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER RESOURCE GROUP user_resource VCPU=0 THREAD_PRIORITY=5;


--echo # 9. Privilege Error  - user with  RESOURCE_GROUP_USER privilege can't alter resource group.
--connection default
GRANT RESOURCE_GROUP_USER ON *.* TO u1;
--connection con1
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER RESOURCE GROUP user_resource  VCPU=0 THREAD_PRIORITY=5;
--connection default
--disconnect con1
DROP USER u1;

--echo # 10. Length of resource group name is greater than maximum allowed limit of identifier names.
--error ER_TOO_LONG_IDENT
ALTER RESOURCE GROUP user_resource_1234567890aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaafffffffff DISABLE;

--echo # 11. Default resource groups can't be altered.
--error ER_DISALLOWED_OPERATION
ALTER RESOURCE GROUP SYS_default VCPU=0;

--error ER_DISALLOWED_OPERATION
ALTER RESOURCE GROUP USR_default VCPU=1;

--echo # 11. Alter VCPU IDS and/or thread priority

SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME='user_resource';
ALTER RESOURCE GROUP user_resource VCPU=0,2;
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME='user_resource';
if ($thr_prio_enable == 1)
{
  ALTER RESOURCE GROUP user_resource THREAD_PRIORITY=3;
  SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME='user_resource';
  ALTER RESOURCE GROUP user_resource VCPU=3 THREAD_PRIORITY=1;
  SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME='user_resource';
}

--echo # DROP resource group tests.

--echo # 1. DROP on a non-existent resource group.
--error ER_RESOURCE_GROUP_NOT_EXISTS
DROP RESOURCE GROUP user_resource1;

--echo # 2. DROP an existing resource group.
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="user_resource";
DROP RESOURCE GROUP user_resource;

--echo # 3. Test FORCE option of resource group.
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=7;
SET RESOURCE GROUP user_resource;
--error ER_RESOURCE_GROUP_BUSY
DROP RESOURCE GROUP user_resource;
SELECT COUNT(*) = 1 FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="user_resource";
DROP RESOURCE GROUP user_resource FORCE;
SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="user_resource";

--echo # 4. Privilege Error - user with RESOURCE_GROUP_USER privilege can't drop resource group.
CREATE USER u1;
GRANT RESOURCE_GROUP_USER ON *.* TO u1;
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3;
--connect(con1,localhost,u1,'',)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
DROP RESOURCE GROUP user_resource;
--connection default
--disconnect con1
DROP RESOURCE GROUP user_resource;
DROP USER u1;

--echo # 10. Length of resource group name is greater than maximum allowed limit of identifier names.
--error ER_TOO_LONG_IDENT
DROP RESOURCE GROUP user_resource_1234567890aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaafffffffff;

--echo # 11. Default resource groups can't be dropped.
--error ER_DISALLOWED_OPERATION
DROP RESOURCE GROUP USR_default;
--error ER_DISALLOWED_OPERATION
DROP RESOURCE GROUP SYS_default;

--echo # SET resource group tests.
# 1. Assigning a non-existing resource group.
--error ER_RESOURCE_GROUP_NOT_EXISTS
SET RESOURCE GROUP user_resource;

--echo # 2. SET current session to a resource group.
CREATE RESOURCE GROUP user_resource TYPE=USER VCPU=1-3 THREAD_PRIORITY=7;
SET RESOURCE GROUP user_resource;
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP="user_resource";

--echo # 3. Assign a system thread to a user resource group.
let $pfs_system_thread_id=`select thread_id from performance_schema.threads where name='thread/innodb/io_write_thread'`;
--replace_result $pfs_system_thread_id pfs_system_thread_id
--error ER_RESOURCE_GROUP_BIND_FAILED
--eval SET RESOURCE GROUP user_resource FOR $pfs_system_thread_id;

--echo # 4. Assign a system thread to system resource group.
CREATE RESOURCE GROUP system_resource TYPE=SYSTEM VCPU=0-2 THREAD_PRIORITY=-1;
--replace_result $pfs_system_thread_id pfs_system_thread_id
--eval SET RESOURCE GROUP system_resource FOR $pfs_system_thread_id;
--replace_result $pfs_system_thread_id pfs_system_thread_id
--eval SELECT THREAD_ID, NAME, RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID=$pfs_system_thread_id;

--echo # 5. Assign a group of threads to a resource group.
let $pfs_system_thread_id1=`select thread_id from performance_schema.threads where name='thread/innodb/io_read_thread'`;
let $pfs_system_thread_id2=`select thread_id from performance_schema.threads where name='thread/innodb/io_ibuf_thread'`;
--replace_result $pfs_system_thread_id1 pfs_system_thread_id1 $pfs_system_thread_id2 pfs_system_thread_id2
--eval SET RESOURCE GROUP system_resource FOR $pfs_system_thread_id1, $pfs_system_thread_id2;
--replace_result $pfs_system_thread_id1 pfs_system_thread_id1 $pfs_system_thread_id2 pfs_system_thread_id2
--eval SELECT COUNT(*) = 2 FROM performance_schema.threads WHERE THREAD_ID IN ($pfs_system_thread_id1, $pfs_system_thread_id2);

--echo # 6. Privilege Tests

CREATE USER u1;
--connect(con1, localhost, u1, '', )
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET RESOURCE GROUP user_resource;

--connection default
GRANT RESOURCE_GROUP_USER ON *.* TO u1;
--connection con1
SET RESOURCE GROUP user_resource;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET RESOURCE GROUP system_resource;
--connection default
--disconnect con1
DROP USER u1;

--echo # 7. SET RESOURCE GROUP on invalid thread id will give warning.
--error ER_INVALID_THREAD_ID
SET RESOURCE GROUP user_resource FOR 77777;

--echo # 8. Disable resource group and assign to the current session - This should give error.
ALTER RESOURCE GROUP user_resource DISABLE;
--error ER_RESOURCE_GROUP_DISABLED
SET RESOURCE GROUP user_resource;
DROP RESOURCE GROUP user_resource FORCE;


--echo # 10. Length of resource group name is greater than maximum allowed limit of identifier names.
--error ER_TOO_LONG_IDENT
SET RESOURCE GROUP user_resource_1234567890aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaafffffffff;

--echo # 11. Do not allow a system resource group to be assigned to user thread.
--replace_regex /thread id \([0-9]+\)/thread id ID/
--error ER_RESOURCE_GROUP_BIND_FAILED
SET RESOURCE GROUP SYS_default;

--echo # Query Hint Tests

--echo # 1. Hint on non-existent Resource Group.
SELECT /*+ RESOURCE_GROUP(rg) */ 1;

--echo # 2. Using system resource group in hint for a non-privilege user should give warning
CREATE USER u1;
--connect(con1, localhost, u1, '', )
--replace_regex /thread id \([0-9]+\)/thread id ID/
SELECT /*+ RESOURCE_GROUP(system_resource) */ 1;
--connection default
--disconnect con1
DROP USER u1;

--echo # 3. Length of resource group name is greater than maximum allowed limit of identifier names.
--echo #    length shall be truncated in this case and warning output to user.
SELECT /*+ RESOURCE GROUP (user_resource_1234567890aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaffffffff#f) */ 1;

--echo # Information Schema and Privilege related tests.
--replace_regex /0-[0-9]+/NUM_VCPUS/
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
CREATE USER u1;
--connect(con1,localhost,u1,'',)
--replace_regex /0-[0-9]+/NUM_VCPUS/
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
--connection default
GRANT RESOURCE_GROUP_USER ON *.* TO u1;
--connection con1
--replace_regex /0-[0-9]+/NUM_VCPUS/
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
--connection default
GRANT RESOURCE_GROUP_ADMIN ON *.* TO u1;
--connection con1
--replace_regex /0-[0-9]+/NUM_VCPUS/
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
--connection default
--disconnect con1
DROP USER u1;
DROP RESOURCE GROUP system_resource FORCE;

--echo # BUG 27638623 - DEBUG ASSERT WHEN EXECUTING CREATE RESOURCE GROUP
--echo #                IN STORED FUNCTION


delimiter |;
--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
CREATE FUNCTION func() RETURNS INT
BEGIN
  CREATE RESOURCE GROUP Batch TYPE = USER VCPU = 2-3;
  RETURN 0;
END|

# Check SET RESOURCE GROUP is allowed in stored procedures.
CREATE RESOURCE GROUP Batch TYPE = USER VCPU = 2-3|
CREATE FUNCTION func() RETURNS INT
BEGIN
  SET RESOURCE GROUP Batch;
  RETURN 0;
END|
delimiter ;|
SELECT func();
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP="Batch";
DROP RESOURCE GROUP Batch FORCE;
DROP FUNCTION func;


#
#  BUG#28448258 - PREPARED STATEMENT FAILS FOR SET RESOURCE GROUP
#
CREATE RESOURCE GROUP test TYPE = USER VCPU=2-3;
SET @rg_name='test';
SET @cmd = CONCAT("SET RESOURCE GROUP ", @rg_name);
SELECT @cmd;
PREPARE stmt FROM @cmd;
EXECUTE stmt;
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP="test";
DROP RESOURCE GROUP test FORCE;
