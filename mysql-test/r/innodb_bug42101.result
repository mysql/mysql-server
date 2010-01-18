set global innodb_commit_concurrency=0;
select @@innodb_commit_concurrency;
@@innodb_commit_concurrency
0
set global innodb_commit_concurrency=1;
ERROR 42000: Variable 'innodb_commit_concurrency' can't be set to the value of '1'
select @@innodb_commit_concurrency;
@@innodb_commit_concurrency
0
set global innodb_commit_concurrency=42;
ERROR 42000: Variable 'innodb_commit_concurrency' can't be set to the value of '42'
select @@innodb_commit_concurrency;
@@innodb_commit_concurrency
0
set global innodb_commit_concurrency=0;
select @@innodb_commit_concurrency;
@@innodb_commit_concurrency
0
set global innodb_commit_concurrency=DEFAULT;
select @@innodb_commit_concurrency;
@@innodb_commit_concurrency
0
