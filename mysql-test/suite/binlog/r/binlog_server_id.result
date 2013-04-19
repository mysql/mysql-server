set global server_id=1;
reset master;
drop table if exists t1,t2,t3;
RESET MASTER;
set global server_id=3;
create table t3 (a int);
include/assert.inc [Server Ids must match (3 == 3)]
RESET MASTER;
set global server_id=2;
create table t2 (a int);
include/assert.inc [Server Ids must match (2 == 2)]
RESET MASTER;
set global server_id=1;
create table t1 (a int);
include/assert.inc [Server Ids must match (1 == 1)]
set global server_id=1;
drop table t1,t2,t3;
