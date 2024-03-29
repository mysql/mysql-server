
--echo # privileges

create database mysqltest1;
use mysqltest1;
create table t1(pub int, priv int);
insert into t1 values(1,2);
analyze table t1;
CREATE USER user1@localhost;
GRANT SELECT (pub) ON mysqltest1.t1 TO user1@localhost;

connect (user1, localhost, user1, ,);
connection user1;

use mysqltest1;
select pub from t1;
--error 1143
select priv from t1;

select * from (select pub from t1) as dt;
explain select * from (select pub from t1) as dt;
--error 1143
select /*+ merge(dt) */ * from (select priv from t1) as dt;
--error 1143
select /*+ no_merge(dt) */ * from (select priv from t1) as dt;
--error 1143
explain select * from (select priv from t1) as dt;

with qn as (select pub from t1) select * from qn;
explain with qn as (select pub from t1) select * from qn;
--error 1143
with qn as (select priv from t1) select /*+ merge(qn) */ * from qn;
--error 1143
with qn as (select priv from t1) select /*+ no_merge(qn) */ * from qn;
--error 1143
explain with qn as (select priv from t1) select * from qn;

with qn2 as (with qn as (select pub from t1) select * from qn)
select * from qn2;
--error 1143
with qn2 as (with qn as (select priv from t1) select * from qn)
select * from qn2;

connection default;
disconnect user1;
drop user user1@localhost;
drop database mysqltest1;
