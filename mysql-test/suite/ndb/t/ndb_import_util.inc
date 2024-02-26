# LOAD DATA line terminators
--let $lt_unix='\n'
--let $lt_windows='\r\n'
if (!$IS_WINDOWS) {
  --let $lt_default=$lt_unix
}
if ($IS_WINDOWS) {
  --let $lt_default=$lt_windows
}

--echo Auto increment coulumn being the primary key
--eval create table t1 (a $col_type not null auto_increment primary key, b int, c bigint) engine ndb
exec $NDB_IMPORT --state-dir=$MYSQLTEST_VARDIR/tmp
      --keep-state --log-level=1 --missing-ai-column
      --fields-terminated-by="::" --csvopt=n
      test
      $MYSQLTEST_VARDIR/tmp/t1.csv >> $NDB_TOOLS_OUTPUT 2>&1;

create table t1ver like t1;

--disable_query_log
eval load data local infile '$MYSQLTEST_VARDIR/tmp/t1.csv' into table t1ver
FIELDS TERMINATED BY '::'
lines terminated by $lt_default(b,c);
--enable_query_log

select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1ver";
select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1";
select count(*) from t1ver;
select count(*) from t1;

select count(*) from t1 x, t1ver y
where x.b = y.b;

drop table t1, t1ver;

--echo Auto increment coulumn being a part of the primary key
--eval create table t1 (a int, b $col_type not null auto_increment, c bigint, primary key(a, b)) engine ndb
exec $NDB_IMPORT --state-dir=$MYSQLTEST_VARDIR/tmp
      --keep-state --log-level=1 --missing-ai-column
      --fields-terminated-by="::" --csvopt=n
      test
      $MYSQLTEST_VARDIR/tmp/t1.csv >> $NDB_TOOLS_OUTPUT 2>&1;

create table t1ver like t1;

--disable_query_log
eval load data local infile '$MYSQLTEST_VARDIR/tmp/t1.csv' into table t1ver
FIELDS TERMINATED BY '::'
lines terminated by $lt_default(a,c);
--enable_query_log

select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1ver";
select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1";
select count(*) from t1ver;
select count(*) from t1;

select count(*) from t1 x, t1ver y
where x.a = y.a;

drop table t1, t1ver;

--echo Auto increment coulumn NOT being a part of the primary key
--eval create table t1 (a int primary key, b $col_type not null auto_increment, c bigint, key(b)) engine ndb
exec $NDB_IMPORT --state-dir=$MYSQLTEST_VARDIR/tmp
      --keep-state --log-level=1 --missing-ai-column
      --fields-terminated-by="::" --csvopt=n
      test
      $MYSQLTEST_VARDIR/tmp/t1.csv >> $NDB_TOOLS_OUTPUT 2>&1;

create table t1ver like t1;

--disable_query_log
eval load data local infile '$MYSQLTEST_VARDIR/tmp/t1.csv' into table t1ver
FIELDS TERMINATED BY '::'
lines terminated by $lt_default(a,c);
--enable_query_log

select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1ver";
select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1";
select count(*) from t1ver;
select count(*) from t1;

select count(*) from t1 x, t1ver y
where x.a = y.a;

drop table t1, t1ver;

--echo Auto increment coulumn being the last column in a table
--eval create table t1 (a int, b bigint, c $col_type not null auto_increment primary key) engine ndb
exec $NDB_IMPORT --state-dir=$MYSQLTEST_VARDIR/tmp
      --keep-state --log-level=1 --missing-ai-column
      --fields-terminated-by="::" --csvopt=n
      test
      $MYSQLTEST_VARDIR/tmp/t1.csv >> $NDB_TOOLS_OUTPUT 2>&1;

create table t1ver like t1;

--disable_query_log
eval load data local infile '$MYSQLTEST_VARDIR/tmp/t1.csv' into table t1ver
FIELDS TERMINATED BY '::'
lines terminated by $lt_default(a,b);
--enable_query_log

select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1ver";
select AUTO_INCREMENT from information_schema.tables where table_schema="test" and table_name="t1";
select count(*) from t1ver;
select count(*) from t1;

select count(*) from t1 x, t1ver y
where x.b = y.b;

drop table t1, t1ver;
