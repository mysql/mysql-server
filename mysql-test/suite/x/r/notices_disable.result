
command ok
check with dropping db which doesn't exists

0 rows affected
check with dropping table which doesn't exists

0 rows affected
check with dropping procedure which doesn't exists

0 rows affected
check with dropping function which doesn't exists

0 rows affected
check with dropping view which doesn't exists

0 rows affected
out of range values for INT,UNSIGN INT

0 rows affected

1 rows affected

1 rows affected

0 rows affected
checking double data type

0 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected
grp	group_concat(a order by a,d+c-ascii(c)-a)
1	1
2	2,3
3	4,5,6,7,8,9
0 rows affected
grp	group_concat(a order by d+c-ascii(c),a)
1	1
2	3,2
3	7,8,4,6,9,5
0 rows affected

0 rows affected
grp	group_concat(c order by c)
1	a
2	b,c
3	C,D,
0 rows affected

0 rows affected
checking character data type
Error is returned instead of warning for insert operation with xplugin

0 rows affected

0 rows affected
Statement prepared

0 rows affected

1 rows affected
length(a)
10
0 rows affected

0 rows affected

0 rows affected
checking directly from IS tables
VARIABLE_NAME	VARIABLE_VALUE
max_join_size	18446744073709551615
0 rows affected
VARIABLE_NAME	VARIABLE_VALUE
concurrent_insert	AUTO
0 rows affected
VARIABLE_NAME	VARIABLE_VALUE
default_storage_engine	InnoDB
0 rows affected
checking errors and warnings with duplicate table

0 rows affected
checking errors and warnings with duplicate table names

0 rows affected
Got expected error: Table 'dup' already exists (code 1050)
@@session.error_count
1
0 rows affected
Level	Code	Message
Error	1050	Table 'dup' already exists
0 rows affected

0 rows affected
checking creating table with if not exists

0 rows affected

0 rows affected
Got expected error: Table 't1' already exists (code 1050)

0 rows affected
a
0 rows affected

0 rows affected
Variable_name	Value
Opened_tables	1
0 rows affected
a
0 rows affected

0 rows affected
try droping mutliplt tables with if exists and check multiple warnings

0 rows affected

0 rows affected
try checking may warnings

0 rows affected
try dropping same table with if exists and observe error/wrarnings
Got expected error: Not unique table/alias: 't1' (code 1066)

0 rows affected
check more than 64 warnings returned

0 rows affected
check no warnings based on max_error_count parameter set

0 rows affected

0 rows affected

0 rows affected
Variable_name	Value
max_error_count	0
0 rows affected

0 rows affected
checking creating temporary table when normal and temp table exists

0 rows affected

1 rows affected
Records: 1  Duplicates: 0  Warnings: 0

1 rows affected
Records: 1  Duplicates: 0  Warnings: 0
a
2
0 rows affected

0 rows affected
a
2
0 rows affected

0 rows affected

0 rows affected
checking data truncation warnings for default values

0 rows affected

1 rows affected
Records: 1  Duplicates: 0  Warnings: 1

0 rows affected
checking data truncation warnings while creating table with expression

0 rows affected

1 rows affected
Records: 1  Duplicates: 0  Warnings: 1

0 rows affected
checking warnings with insert and select operations
below insert into t1 returns 17775 warnings,checking only limited warnings as test takes more time to render the warnings

0 rows affected

0 rows affected

8 rows affected
Records: 8  Duplicates: 0  Warnings: 0

8 rows affected
Records: 8  Duplicates: 0  Warnings: 0

0 rows affected
checking for more than default 64 warnings

0 rows affected

65536 rows affected
Records: 65536  Duplicates: 0  Warnings: 17775

0 rows affected

0 rows affected
checking warnigns with prepare and execute statmenets

0 rows affected

0 rows affected

1 rows affected
1
1
0 rows affected

0 rows affected
Statement prepared
1
1
0 rows affected
1
1
0 rows affected

0 rows affected

0 rows affected
Statement prepared
1
1
0 rows affected
1
1
0 rows affected

0 rows affected

0 rows affected
checking warnings with group by when sql_mode is no_engine_substitution

0 rows affected

0 rows affected

0 rows affected

0 rows affected

2 rows affected
Records: 2  Duplicates: 0  Warnings: 0

2 rows affected
Records: 2  Duplicates: 0  Warnings: 0
c2
aaa
aaa
0 rows affected
c2
aaa
0 rows affected

0 rows affected
setting back to default of sql_mode

0 rows affected
checking with group by where both error and warnings expected with default sql_mode

0 rows affected

0 rows affected

0 rows affected

2 rows affected
Records: 2  Duplicates: 0  Warnings: 0

2 rows affected
Records: 2  Duplicates: 0  Warnings: 0
Got expected error: Expression #1 of SELECT list is not in GROUP BY clause and contains nonaggregated column 'test.t1.c1' which is not functionally dependent on columns in GROUP BY clause; this is incompatible with sql_mode=only_full_group_by (code 1055)
expect both error and warning
change result file once Bug#21524666 is fixed
c2
aaa
0 rows affected

0 rows affected
checking warnings with rollback on non-transactional tables

0 rows affected

0 rows affected

0 rows affected

1 rows affected

1 rows affected

0 rows affected
Level	Code	Message
Warning	1196	Some non-transactional changed tables couldn't be rolled back
0 rows affected
Level	Code	Message
0 rows affected
@@warning_count	@@error_count
1	0
0 rows affected
n
4
5
0 rows affected
Level	Code	Message
0 rows affected
@@warning_count
0
0 rows affected

0 rows affected
expect 2 warnings for insert related to truncation and incorrect date value

0 rows affected

0 rows affected

0 rows affected

1 rows affected
Records: 1  Duplicates: 0  Warnings: 2

0 rows affected

0 rows affected
checking warnings while creating index which is more than row size

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected
below statement throws warning as Error and actual error is missing
change result file once Bug#21529270 fixed
Got expected error: Specified key was too long; max key length is 3072 bytes (code 1071)

0 rows affected

0 rows affected

0 rows affected

0 rows affected
checking warnings with different innodb file format

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected
checking warnings with different innodb row format

0 rows affected
expect one Error and Warning
modify the result file once Bug#21524666 is fixed
Got expected error: Table storage engine for 't2' doesn't have this option (code 1031)
expect 2 warnings and 1 Error
modify the result file once Bug#21524666 is fixed
Got expected error: Table storage engine for 't2' doesn't have this option (code 1031)
expect 1 warning and 2 Error
modify the result file once Bug#21524666 is fixed
Got expected error: Incorrect index name 'GEN_CLUST_INDEX' (code 1280)
Got expected error: Cannot disable notice account_expired (code 5164)
Mysqlx.Ok {
  msg: "bye!"
}
ok
Try Disable warnings

command ok
Try Disable account_expired 
Got expected error: Cannot disable notice account_expired (code 5164)
Try Disable generated_insert_id 
Got expected error: Cannot disable notice generated_insert_id (code 5164)
Try Disable rows_affected 
Got expected error: Cannot disable notice rows_affected (code 5164)
Try Disable produced_message 
Got expected error: Cannot disable notice produced_message (code 5164)
expect Mysqlx_stmt_disable_notices=5 and remaining are 0
Variable_name	Value
Mysqlx_notice_global_sent	1
Mysqlx_notice_other_sent	1
Mysqlx_notice_warning_sent	0
Mysqlx_stmt_disable_notices	5
Mysqlx_stmt_enable_notices	0
Mysqlx_stmt_list_notices	0
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
create user xplugin@localhost identified by 'xplugin';
Try Disable warnings

command ok
Try Disable account_expired 
Got expected error: Cannot disable notice account_expired (code 5164)
Try Disable generated_insert_id 
Got expected error: Cannot disable notice generated_insert_id (code 5164)
Try Disable rows_affected 
Got expected error: Cannot disable notice rows_affected (code 5164)
Try Disable produced_message 
Got expected error: Cannot disable notice produced_message (code 5164)
expect Mysqlx_stmt_disable_notices=5 and remaining are 0
Variable_name	Value
Mysqlx_notice_global_sent	1
Mysqlx_notice_other_sent	1
Mysqlx_notice_warning_sent	0
Mysqlx_stmt_disable_notices	5
Mysqlx_stmt_enable_notices	0
Mysqlx_stmt_list_notices	0
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok

command ok
checking warnings with procedure execution

0 rows affected
@start_global_value
1024
0 rows affected

0 rows affected
@start_session_value
1024
0 rows affected

0 rows affected

1 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected
Got expected error: Oops in proc_1 (code 1644)

0 rows affected
@@session.max_error_count
5
0 rows affected
Got expected error: Oops in proc_1 (code 1644)

0 rows affected
@@session.max_error_count
7
0 rows affected
Got expected error: Oops in proc_1 (code 1644)

0 rows affected
@@session.max_error_count
9
0 rows affected
Got expected error: Oops in proc_1 (code 1644)

0 rows affected

0 rows affected
@@global.max_error_count
1024
0 rows affected

0 rows affected
@@session.max_error_count
1024
0 rows affected
checking warnings while creating index with more than row size

0 rows affected

0 rows affected

0 rows affected

0 rows affected

0 rows affected
below statement throwing warning as an Error and actual error is missing
Change result file once Bug#21529270 fixed
Got expected error: Specified key was too long; max key length is 3072 bytes (code 1071)

0 rows affected

0 rows affected

0 rows affected

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
Disable multiple notices both read-only and read-write with single StmtExecute,expect error from the first error is sees
expect error from account_expired onwards as it is read-only though warnings returns success as it is rea-write
Got expected error: Cannot disable notice account_expired (code 5164)
notice	enabled
warnings	1
group_replication/membership/quorum_loss	0
group_replication/membership/view	0
group_replication/status/role_change	0
group_replication/status/state_change	0
account_expired	1
generated_insert_id	1
rows_affected	1
produced_message	1
command ok
expect error as disabling account_expired returns error as expected since it is read-ony
Got expected error: Cannot disable notice account_expired (code 5164)
notice	enabled
warnings	1
group_replication/membership/quorum_loss	0
group_replication/membership/view	0
group_replication/status/role_change	0
group_replication/status/state_change	0
account_expired	1
generated_insert_id	1
rows_affected	1
produced_message	1
command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
drop user xplugin@localhost;
