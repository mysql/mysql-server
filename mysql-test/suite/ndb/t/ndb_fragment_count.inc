#
# test fragment creation
#
#
create table t1
  (a bigint, b bigint, c bigint, d bigint,
   primary key (a,b,c,d))
  engine=ndb;
insert into t1 values
  (1,2,3,4),(2,3,4,5),(3,4,5,6),
  (3,2,3,4),(1,3,4,5),(2,4,5,6),
  (1,2,3,5),(2,3,4,8),(3,4,5,9),
  (3,2,3,5),(1,3,4,8),(2,4,5,9),
  (1,2,3,6),(2,3,4,6),(3,4,5,7),
  (3,2,3,6),(1,3,4,6),(2,4,5,7),
  (1,2,3,7),(2,3,4,7),(3,4,5,8),
  (3,2,3,7),(1,3,4,7),(2,4,5,8),
  (1,3,3,4),(2,4,4,5),(3,5,5,6),
  (3,3,3,4),(1,4,4,5),(2,5,5,6),
  (1,3,3,5),(2,4,4,8),(3,5,5,9),
  (3,3,3,5),(1,4,4,8),(2,5,5,9),
  (1,3,3,6),(2,4,4,6),(3,5,5,7),
  (3,3,3,6),(1,4,4,6),(2,5,5,7),
  (1,3,3,7),(2,4,4,7),(3,5,5,8),
  (3,3,3,7),(1,4,4,7),(2,5,5,8);

select count(*) into @cnt_start
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

alter table t1 algorithm=copy,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
select count(*) into @cnt_one_per_ldm_per_node
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

alter table t1 algorithm=copy,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_LDM";
select count(*) into @cnt_one_per_ldm_per_nodegroup
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

alter table t1 algorithm=copy,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_NODE";
select count(*) into @cnt_one_per_node
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

alter table t1 algorithm=copy,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_NODE";
select count(*) into @cnt_one_per_nodegroup
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

drop table t1;

select @cnt_start = @cnt_one_per_ldm_per_node;
select @cnt_one_per_ldm_per_node > @cnt_one_per_ldm_per_nodegroup;
select @cnt_one_per_ldm_per_node > @cnt_one_per_node;
select @cnt_one_per_nodegroup <= @cnt_one_per_ldm_per_nodegroup;
select @cnt_one_per_nodegroup = 1;

###
#
# Test online alter table
#
create table t1 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

create table t2 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t2' order by node_id, fragment_num, block_instance;

create table t3 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_LDM";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t3' order by node_id, fragment_num, block_instance;

create table t4 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_NODE";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t4' order by node_id, fragment_num, block_instance;

create table t5 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_NODE";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t5' order by node_id, fragment_num, block_instance;

select count(*) into @cnt_t1_0
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select count(*) into @cnt_t2_0
from information_schema.partitions
where table_schema = 'test' and table_name = 't2';

select count(*) into @cnt_t3_0
from information_schema.partitions
where table_schema = 'test' and table_name = 't3';

select count(*) into @cnt_t4_0
from information_schema.partitions
where table_schema = 'test' and table_name = 't4';

select count(*) into @cnt_t5_0
from information_schema.partitions
where table_schema = 'test' and table_name = 't5';

##
#
# reorganize partitions work for PARTITION_BALANCE
#   but should do nothing in these cases
#
alter table t1 algorithm=inplace, reorganize partition;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

alter table t2 algorithm=inplace, reorganize partition;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t2' order by node_id, fragment_num, block_instance;

alter table t3 algorithm=inplace, reorganize partition;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t3' order by node_id, fragment_num, block_instance;

alter table t4 algorithm=inplace, reorganize partition;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t4' order by node_id, fragment_num, block_instance;

alter table t5 algorithm=inplace, reorganize partition;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t5' order by node_id, fragment_num, block_instance;

select count(*) into @cnt_t1_1
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select count(*) into @cnt_t2_1
from information_schema.partitions
where table_schema = 'test' and table_name = 't2';

select count(*) into @cnt_t3_1
from information_schema.partitions
where table_schema = 'test' and table_name = 't3';

select count(*) into @cnt_t4_1
from information_schema.partitions
where table_schema = 'test' and table_name = 't4';

select count(*) into @cnt_t5_1
from information_schema.partitions
where table_schema = 'test' and table_name = 't5';

select @cnt_t1_0 = @cnt_t1_1;
select @cnt_t2_0 = @cnt_t2_1;
select @cnt_t3_0 = @cnt_t3_1;
select @cnt_t4_0 = @cnt_t4_1;
select @cnt_t5_0 = @cnt_t5_1;

##
#
# but add partition does not!
#
alter table t1 algorithm=inplace, add partition partitions 1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

alter table t2 algorithm=inplace, add partition partitions 1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t2' order by node_id, fragment_num, block_instance;

alter table t3 algorithm=inplace, add partition partitions 1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t3' order by node_id, fragment_num, block_instance;

alter table t4 algorithm=inplace, add partition partitions 1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t4' order by node_id, fragment_num, block_instance;

alter table t5 algorithm=inplace, add partition partitions 1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t5' order by node_id, fragment_num, block_instance;

select count(*) into @cnt_t1_2
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select count(*) into @cnt_t2_2
from information_schema.partitions
where table_schema = 'test' and table_name = 't2';

select count(*) into @cnt_t3_2
from information_schema.partitions
where table_schema = 'test' and table_name = 't3';

select count(*) into @cnt_t4_2
from information_schema.partitions
where table_schema = 'test' and table_name = 't4';

select count(*) into @cnt_t5_2
from information_schema.partitions
where table_schema = 'test' and table_name = 't5';

select @cnt_t1_2;
select @cnt_t2_2;
select @cnt_t3_2;
select @cnt_t4_2;
select @cnt_t5_2;

select @cnt_t1_0 < @cnt_t1_2;
select @cnt_t2_0 < @cnt_t2_2;
select @cnt_t3_0 < @cnt_t3_2;
select @cnt_t4_0 < @cnt_t4_2;
select @cnt_t5_0 < @cnt_t5_2;

drop table t1, t2, t3, t4, t5;

create table t1 (a int primary key) engine ndb
  COMMENT="NDB_TABLE=NOLOGGING=1";
--error ER_ILLEGAL_HA_CREATE_OPTION
alter table t1 algorithm=inplace,
  COMMENT="NDB_TABLE=NOLOGGING=0";
drop table t1;

##
#
# to add partitions to t2, we can go from low number to a high number.
create table t1 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
alter table t1 algorithm=inplace,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

select count(*) into @cnt_t1_3
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select @cnt_t1_0 = @cnt_t1_3;
#alter table t1 algorithm=inplace, add partition partitions 1;

select count(*) into @cnt_t1_4
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select @cnt_t1_0 < @cnt_t1_4;

drop table t1;

create table t1 (a bigint, b bigint, c bigint, d bigint, primary key (a,b,c,d))
  engine=ndb comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
alter table t1 algorithm=inplace,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_NODE";

alter table t1 algorithm=copy,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_NODE,NOLOGGING=1";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

show create table t1;

alter table t1 algorithm=inplace,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_NODE";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

select count(*) into @cnt_t1_5
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select @cnt_t1_5;

show create table t1;

alter table t1 algorithm=inplace,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_LDM";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

select count(*) into @cnt_t1_6
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select @cnt_t1_6;

show create table t1;

alter table t1 algorithm=inplace,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

select count(*) into @cnt_t1_7
from information_schema.partitions
where table_schema = 'test' and table_name = 't1';

select @cnt_t1_7;

show create table t1;
--let ndb_desc_opts= -d test t1
--source suite/ndb/include/ndb_desc_print.inc

drop table t1;

#Verify that it isn't possible to create no logging table with disk fields
--error ER_ILLEGAL_HA_CREATE_OPTION
create table t1 (a bigint primary key, b bigint storage disk) engine ndb
  comment="NDB_TABLE=NOLOGGING=1";
show warnings;

#Verify that unique indexes gets also correct partition balance
create table t2 (a bigint primary key, b bigint unique) engine ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_NODE";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t2' order by node_id, fragment_num, block_instance;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name like '%unique' order by node_id, fragment_num, block_instance;

#Verify that unique indexes gets also correct partition balance also after
#alter table with copy algorithm
alter table t2 algorithm=copy,
  COMMENT="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_NODE";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t2' order by node_id, fragment_num, block_instance;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name like '%unique' order by node_id, fragment_num, block_instance;
drop table t2;

#Verify that unique indexes gets also correct partition balance also after
#alter table with inplace algorithm. In fact it doesn't since unique indexes
#are not reorganised as part of ALTER TABLE reorg, this is a known bug that
#should be fixed.
create table t2 (a bigint primary key, b bigint unique) engine ndb
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RA_BY_NODE";
alter table t2 algorithm=inplace,
  COMMENT="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_NODE";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t2' order by node_id, fragment_num, block_instance;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name like '%unique' order by node_id, fragment_num, block_instance;
drop table t2;

#A few tests with READ_BACKUP flag set
create table t1 (a bigint primary key, b bigint unique) engine ndb
  comment="NDB_TABLE=READ_BACKUP=1";
show create table t1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name like '%unique' order by node_id, fragment_num, block_instance;

alter table t1 algorithm=inplace,
  comment="NDB_TABLE=PARTITION_BALANCE=FOR_RP_BY_LDM";
show create table t1;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name like '%unique' order by node_id, fragment_num, block_instance;

drop table t1;
create table t1 (a bigint primary key, b bigint unique) engine ndb;
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;
alter table t1 algorithm=copy,
  COMMENT="NDB_TABLE=READ_BACKUP=1";
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;

drop table t1;

create table t1 (a bigint primary key) engine ndb
partition by key();
select node_id, fragment_num, block_instance from ndbinfo.memory_per_fragment
where fq_name = 'test/def/t1' order by node_id, fragment_num, block_instance;
drop table t1;

create table t1 (a bigint primary key) engine ndb
comment="NDB_TABLE=READ_BACKUP=0"
partition by key();
alter table t1 algorithm=inplace, comment="NDB_TABLE=READ_BACKUP=1";
alter table t1 algorithm=inplace, comment="NDB_TABLE=READ_BACKUP=1";
alter table t1 algorithm=inplace, comment="NDB_TABLE=READ_BACKUP=0";
alter table t1 algorithm=inplace, comment="NDB_TABLE=READ_BACKUP=0";
drop table t1;
