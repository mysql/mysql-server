ok 1 - use My::SafeProcess::Base;
ok 2 - safe_fork
ok 3 # skip forkbomb
1..3
ok 1 - use My::File::Path;
ok 2 - Check subdir is created
ok 3 - Check test_todir is created
ok 4 - Check 'test1' is created
ok 5 - Check 'test2' is created
ok 6 - Check 'test3' is created
ok 7 - Check testdir is gone
ok 8 - Check test_todir is gone
1..8
ok 1 - use My::Find;
========================================
mysqld_exe found
========================================
mysql_exe found
========================================
charset_dir found
========================================
1..1
ok 1 - use My::Options;
test0
: ['--binlog-format=row', '--binlog-format=ms']
: ['--binlog-format=row', '--binlog-format=statement']
: ['--binlog-format=statement']
ok 2
SET GLOBAL binlog_format=statement

test1
: ['--binlog-format=row', '--binlog-format=statement']
: ['--binlog-format=row', '--binlog-format=mixed']
: ['--binlog-format=mixed']
ok 3
SET GLOBAL binlog_format=mixed

test2
: ['--binlog-format=row', '--binlog-format=mixed']
: ['--binlog-format=row', '--binlog-format=statement']
: ['--binlog-format=statement']
ok 4
SET GLOBAL binlog_format=statement

test3
: ['--binlog-format=mixed', '--binlog-format=row']
: ['--binlog-format=statement', '--binlog-format=row']
: ['']
ok 5


test4
: ['--binlog-format=row']
: ['']
: ['--binlog-format=default']
ok 6
SET GLOBAL binlog_format=default

test5
: ['']
: ['--binlog-format=row']
: ['--binlog-format=row']
ok 7
SET GLOBAL binlog_format=row

test6
: ['']
: ['max_binlog_size=1']
: ['--max_binlog_size=1']
ok 8
SET GLOBAL max_binlog_size=1

test7
: ['max_binlog_size=1']
: ['max_binlog_size=1']
: ['']
ok 9


test8
: ['max_binlog_size=1']
: ['']
: ['--max_binlog_size=default']
ok 10
SET GLOBAL max_binlog_size=default

test9
: ['']
: ['max_binlog_size=1', '--binlog-format=row']
: ['--max_binlog_size=1', '--binlog-format=row']
ok 11
SET GLOBAL binlog_format=row; SET GLOBAL max_binlog_size=1

test10
: ['--binlog-format=statement']
: ['max_binlog_size=1', '--binlog-format=row']
: ['--max_binlog_size=1', '--binlog-format=row']
ok 12
SET GLOBAL binlog_format=row; SET GLOBAL max_binlog_size=1

test11
: ['--binlog-format=statement']
: ['max_binlog_size=1', '--binlog-format=statement']
: ['--max_binlog_size=1']
ok 13
SET GLOBAL max_binlog_size=1

test12
: ['--binlog-format=statement']
: ['max_binlog_size=1', '--binlog-format=statement']
: ['--max_binlog_size=1']
ok 14
SET GLOBAL max_binlog_size=1

test13
: ['--binlog-format=statement']
: ['--relay-log=/path/to/a/relay-log', '--binlog-format=row']
: ['--relay-log=/path/to/a/relay-log', '--binlog-format=row']
ok 15
SET GLOBAL binlog_format=row; SET GLOBAL relay_log=/path/to/a/relay-log

test14
: ['--binlog-format=statement']
: ['--relay-log=/path/to/a/relay-log', 'max_binlog_size=1']
: ['--max_binlog_size=1', '--relay-log=/path/to/a/relay-log', '--binlog-format=default']
ok 16
SET GLOBAL binlog_format=default; SET GLOBAL max_binlog_size=1; SET GLOBAL relay_log=/path/to/a/relay-log

test15
: ['--slow-query-log=0']
: ['--slow-query-log']
: ['--slow-query-log']
ok 17
SET GLOBAL slow_query_log=

1..17
ok 1 - use My::File::Path;
ok 2 - Check subdir is created
ok 3 - Check testdir is gone
ok 4 - Check subdir is created
ok 5 - Check one dir was chmoded
ok 6 - Check testdir is gone
ok 7 - Check subdir is created
ok 8 - Check one file was chmoded
ok 9 - Check testdir is gone
1..9
ok 1 - use My::SafeProcess;
ok 2 - check exit_status, 32
ok 3 - concurrent, 16
ok 4 - check stdout
ok 5 - check stderr
ok 6 - check stdout
ok 7 - check stderr
ok 8 - check stdout
ok 9 - check stderr
ok 10 - check stdout
ok 11 - check stderr
Hello stdout
ok 12 - check stdout
ok 13 - check stderr
Hello stderr
ok 14 - check stdout
ok 15 - check stderr
ok 16 - check stdout
ok 17 - check stderr
ok 18 - check stdout
ok 19 - check stderr
1..19
ok 1 - use My::Config;
ok 2 - config is a My::Config
[mysqld]
option1=values2
option2=value4
option4
basedir=thebasedir

[mysqld_1]

[mysqld_2]

[mysqld.9]

[client]
socket=asocketpath

ok 3 - group mysqld_2 exists
ok 4 - group mysqld_1 exists
ok 5 - group mysqld.9 exists
ok 6 - group mysqld.9 has suffix .9
ok 7 - group mysqld exists
ok 8 - group client exists
ok 9 - group mysqld_3 does not exist
ok 10 - options in [mysqld] is 4
ok 11 - group [nonexist] is not defined
ok 12 - 5 groups
ok 13 - checking groups 1
ok 14 - checking groups 2
ok 15 - checking groups 3
ok 16 - checking groups 4
ok 17 - checking groups 5
ok 18 - 4 groups like mysqld
ok 19 - checking like("mysqld") 1
ok 20 - checking like("mysqld") 2
ok 21 - checking like("mysqld") 3
ok 22 - checking like("mysqld") 4
ok 23 - checking like("not")
ok 24 - first_like
ok 25 - mysqld_option4 exists, does not have a value
ok 26 - mysqld_option4 exists
ok 27 - mysqld_option2 exists
ok 28 - mysqld_option5 does not exists
ok 29 - config2 is a My::Config
ok 30 - test.cnf is equal to test2.cnf
ok 31 - config3 is a My::Config
[mysqld]
option1=values3
option2=value4
option4
basedir=anotherbasedir

[client]
socket=asocketpath

ok 32 - mysqld_basedir has been overriden by value in test_include.cnf
ok 33 - mysqld_option1 has been overriden by value in test_include.cnf
ok 34 - mysqld_option2 is from included file
ok 35 - client.socket is from included file
ok 36 - mysqld_option4 exists, does not have a value
[mysqld]
option1=values3
option2=value4
option4
basedir=anotherbasedir

[client]
socket=asocketpath


1..36
ok 1 - use My::ConfigFactory;
ok 2 - group mysqld.master exists
ok 3 - group mysqld.1 exists
ok 4 - group client exists
ok 5 - group mysqld.3 does not exist
ok 6 - group like 'mysqld' exists
ok 7 - mysqld.1.\#host has been generated
ok 8 - client.host has been generated
ok 9 - client.host is same as mysqld.master.host
ok 10 - 'character-sets-dir' generated
ok 11 - 'MASTER_MY_PORT' generated
ok 12 - group like 'mysqld' exists
1..12
