# This is a MyISAM only test and hence should retain force_myisam_default.
--source include/force_myisam_default.inc
--source include/have_myisam.inc
# Skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
--source include/elide_costs.inc

# t/index_merge_myisam.test
#
# Index merge tests
#
# Last update:
# 2006-08-07 ML test refactored (MySQL 5.1)
#               Main code of several index_merge tests
#                            -> include/index_merge*.inc
#               wrapper t/index_merge_innodb.test sources now several
#               include/index_merge*.inc files
#

SET @global_optimizer_switch_saved = @@global.optimizer_switch;

let $engine_type= MyISAM;
# MyISAM supports Merge tables
let $merge_table_support= 1;

--source include/index_merge1.inc
--source include/index_merge_ror.inc
--source include/index_merge2.inc
--source include/index_merge_2sweeps.inc
--source include/index_merge_ror_cpk.inc

--echo #
--echo # Generic @@optimizer_switch tests (move those into a separate file if
--echo #  we get another @@optimizer_switch user)
--echo #

select @@optimizer_switch;
set @default_optimizer_switch = @@optimizer_switch;

set optimizer_switch='index_merge=off,index_merge_union=off';
select @@optimizer_switch;

set optimizer_switch='index_merge_union=on';
select @@optimizer_switch;

set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_sort_union=off';
select @@optimizer_switch;

set optimizer_switch=4;

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch=NULL;

set optimizer_switch=@default_optimizer_switch;
--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge=index_merge';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge=on,but...';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge=';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='on';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge=on,index_merge=off';

--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge_union=on,index_merge_union=default';

set optimizer_switch=@default_optimizer_switch;
--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='index_merge=on,index_merge=off';

set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge=off,index_merge_union=off';
select @@optimizer_switch;
set optimizer_switch=@default_optimizer_switch;

# Check setting defaults for global vars
--replace_result hypergraph_optimizer=on hypergraph_optimizer=off
select @@global.optimizer_switch;
set @@global.optimizer_switch=@default_optimizer_switch;
select @@global.optimizer_switch;

--echo #
--echo # Check index_merge's @@optimizer_switch flags
--echo #
select @@optimizer_switch;

create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
analyze table t0;

create table t1 (a int, b int, c int, filler char(100),
                 key(a), key(b), key(c));
insert into t1 select
  A.a * B.a*10 + C.a*100,
  A.a * B.a*10 + C.a*100,
  A.a,
  'filler'
from t0 A, t0 B, t0 C;
analyze table t1;

--echo This should use union:
--replace_regex $elide_costs
explain select * from t1 where a=1 or b=1;

--echo This should use ALL:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge=off';
--replace_regex $elide_costs
explain select * from t1 where a=1 or b=1;

--echo This should use sort-union:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_union=off';
--replace_regex $elide_costs
explain select * from t1 where a=1 or b=1;

--echo This will use sort-union:
set optimizer_switch=@default_optimizer_switch;
--replace_regex $elide_costs
explain select * from t1 where a<1 or b <1;

--echo This should use ALL:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_sort_union=off';
--replace_regex $elide_costs
explain select * from t1 where a<1 or b <1;


--echo This should use ALL:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge=off';
--replace_regex $elide_costs
explain select * from t1 where a<1 or b <1;

--echo This will use sort-union:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_union=off';
--replace_regex $elide_costs
explain select * from t1 where a<1 or b <1;

alter table t1 add d int, add key(d);
update t1 set d=a;
analyze table t1;

--echo This will use sort_union:
set optimizer_switch=@default_optimizer_switch;
--replace_regex $elide_costs
explain select * from t1 where (a=3 or b in (1,2)) and (c=3 or d=4);

--echo And if we disable sort_union, union:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_sort_union=off';
--replace_regex $elide_costs
explain select * from t1 where (a=3 or b in (1,2)) and (c=3 or d=4);

drop table t1;

# Now test that intersection can be disabled
create table t1 (
  a int, b int, c int,
  filler1 char(200), filler2 char(200),
  key(a),key(b),key(c)
);

insert into t1
select A.a+10*B.a, A.a+10*B.a, A.a+10*B.a+100*C.a, 'foo', 'bar'
from t0 A, t0 B, t0 C, t0 D where D.a<5;
analyze table t1;

--echo This should be intersect:
set optimizer_switch=@default_optimizer_switch;
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10;

--echo No intersect when index_merge is disabled:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge=off';
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10;

--echo No intersect if it is disabled:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_intersection=off';
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10;

--echo Do intersect when union was disabled
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_union=off';
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10;

--echo Do intersect when sort_union was disabled
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_sort_union=off';
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10;

# Now take union-of-intersection and see how we can disable parts of it
--echo This will use intersection inside a union:
set optimizer_switch=@default_optimizer_switch;
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10 or c=10;

--echo Should be only union left:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_intersection=off';
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10 or c=10;

--echo This will switch to sort-union (intersection will be gone, too,
--echo   thats a known limitation:
set optimizer_switch=@default_optimizer_switch;
set optimizer_switch='index_merge_union=off';
--replace_regex $elide_costs
explain select * from t1 where a=10 and b=10 or c=10;

set optimizer_switch=@default_optimizer_switch;
show variables like 'optimizer_switch';

# Reset the global switch to default.
set @@global.optimizer_switch=@global_optimizer_switch_saved;

drop table t0, t1;
