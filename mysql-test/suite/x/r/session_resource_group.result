CREATE RESOURCE GROUP my_group TYPE=USER;

USR_default
===========
RUN SET RESOURCE GROUP USR_default

0 rows affected
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'USR_default']
RUN SELECT /*+ RESOURCE_GROUP(SYS_default) */ 1 AS value
value
1
0 rows affected
Warnings generated:
WARNING | 3661 | Unable to bind resource group SYS_default with thread id (ID).(System resource group can't be bound with a session thread).
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'USR_default']
RUN SELECT /*+ RESOURCE_GROUP(my_group) */ 1 AS value
value
1
0 rows affected
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'USR_default']
RUN SELECT /*+ RESOURCE_GROUP(USR_default) */ 1 AS value
value
1
0 rows affected
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'USR_default']
Mysqlx.Ok {
  msg: "bye!"
}
ok

my_group
========
RUN SET RESOURCE GROUP my_group

0 rows affected
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'my_group']
RUN SELECT /*+ RESOURCE_GROUP(SYS_default) */ 1 AS value
value
1
0 rows affected
Warnings generated:
WARNING | 3661 | Unable to bind resource group SYS_default with thread id (ID).(System resource group can't be bound with a session thread).
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'my_group']
RUN SELECT /*+ RESOURCE_GROUP(my_group) */ 1 AS value
value
1
0 rows affected
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'my_group']
RUN SELECT /*+ RESOURCE_GROUP(USR_default) */ 1 AS value
value
1
0 rows affected
RUN SELECT RESOURCE_GROUP FROM performance_schema.threads WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
Verify [Resource group needs to be 'my_group']
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP RESOURCE GROUP my_group FORCE;
