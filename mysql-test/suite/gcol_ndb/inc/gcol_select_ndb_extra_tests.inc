# set default_storage_engine = 'ndbcluster';


-- echo TESTS FROM gcol_select_ndb_extra_tests.inc

# t1 has an auto-increment pk
create table t1
  (t1_seq_pk int auto_increment not null primary key,
   t1_base_col int,
   t1_vir_gcol int generated always as (-t1_base_col) virtual,
   t1_sto_gcol int generated always as (-t1_base_col) stored,
   index (t1_sto_gcol));

insert into t1 (t1_base_col) values (2), (1), (1), (3), (NULL);

# t2 has a visible pk and a unique index on a stored gcol
create table t2
  (t2_pk int primary key,
   t2_vir_gcol int generated always as (-t2_pk) virtual,
   t2_sto_gcol_unique int generated always as (-t2_pk) stored unique);

insert into t2 (t2_pk) values (1),(2),(3),(4),(5),(6);

# t3 has a hidden pk, and the virtual column is after the stored column
create table t3
 (t3_base_col int,
  t3_sto_gcol int generated always as (-t3_base_col) stored,
  t3_vir_gcol int generated always as (-t3_base_col) virtual,
  index(t3_sto_gcol)
 );

insert into t3 (t3_base_col) values (2), (1), (1), (3), (NULL);

# t4 is like t1 but changes relative order of stored & virtual gcols
create table t4
  (t4_seq_pk int auto_increment not null primary key,
   t4_base_col int,
   t4_sto_gcol int generated always as (-t4_base_col) stored,
   t4_vir_gcol int generated always as (-t4_base_col) virtual,
   index (t4_sto_gcol));

insert into t4 (t4_base_col) values (2), (1), (1), (3), (NULL);

# t5 is like t4 but moves base_col relative to gcols
create table t5
  (t5_seq_pk int auto_increment not null primary key,
   t5_sto_gcol int generated always as (-t5_base_col) stored,
   t5_vir_gcol int generated always as (-t5_base_col) virtual,
   t5_base_col int,
   index (t5_sto_gcol));

insert into t5 (t5_base_col) values (2), (1), (1), (3), (NULL);


--echo # TABLE SCAN
--sorted_result
select * from t2;

--echo # TABLE SCAN WITH MASK
--sorted_result
select t2_pk, t2_vir_gcol from t2;
--sorted_result
select t2_sto_gcol_unique from t2;

--echo # RANGE SCANS
--sorted_result
select * from t2 where t2_pk = 2 or t2_pk = 4;
--sorted_result
select t2_vir_gcol from t2 where t2_pk = 2 or t2_pk = 4;
--sorted_result
select * from t2 where t2_sto_gcol_unique = -2 or t2_sto_gcol_unique = -4;
--sorted_result
select t2_vir_gcol from t2 where t2_sto_gcol_unique = -2 or t2_sto_gcol_unique = -4;

--echo # Join t1 to t2
# set debug = 'O,/Users/jdd/Desktop/TraceFiles/tf-mtr:t:N:d';
--sorted_result
select t1_base_col from t1
  join t2 on (t1_sto_gcol = t2_sto_gcol_unique)
  where t2_sto_gcol_unique between -2 and -1 ;
# set debug = '';

--echo # As above but join t3 to t2
--sorted_result
select t3_base_col from t3
  join t2 on (t3_sto_gcol = t2_sto_gcol_unique)
  where t2_sto_gcol_unique between -2 and -1 ;

--echo # Join t4 to t2
--sorted_result
select t4_base_col from t4
  join t2 on (t4_sto_gcol = t2_sto_gcol_unique)
  where t2_sto_gcol_unique between -2 and -1 ;

--echo # Join t5 to t2
--sorted_result
select t5_base_col from t5
  join t2 on (t5_sto_gcol = t2_sto_gcol_unique)
  where t2_sto_gcol_unique between -2 and -1 ;


--echo # Subquery transformed to semi-join
--sorted_result
select * from t1
where t1_sto_gcol in
  (select t2_sto_gcol_unique from t2
   where t2_sto_gcol_unique between -2 and -1);



# Extra cleanup
drop table t4;
drop table t5;
