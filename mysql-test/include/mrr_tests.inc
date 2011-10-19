
create table t1(a int);
show create table t1;
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2(a int);
insert into t2 select A.a + 10*(B.a + 10*C.a) from t1 A, t1 B, t1 C;


create table t3 (
  a char(8) not null, b char(8) not null, filler char(200),
  key(a)
);
insert into t3 select @a:=concat('c-', 1000+ A.a, '=w'), @a, 'filler' from t2 A;
insert into t3 select concat('c-', 1000+A.a, '=w'), concat('c-', 2000+A.a, '=w'), 
                      'filler-1' from t2 A;
insert into t3 select concat('c-', 1000+A.a, '=w'), concat('c-', 3000+A.a, '=w'), 
                      'filler-2' from t2 A;

# Test empty result set
select a,filler from t3 where a >= 'c-9011=w';

# Ok, t3.ref_length=6, limit is 64 => 10 elements fit into the buffer
# Test the cases when buffer gets exhausted at different points in source
# intervals:

# 1. Split is in the middle of the range
select a,filler from t3 where a >= 'c-1011=w' and a <= 'c-1015=w'; 

# 2. Split is at range edge 
select a,filler from t3 where (a>='c-1011=w' and a <= 'c-1013=w') or
                              (a>='c-1014=w' and a <= 'c-1015=w');

# 3. Split is at range edge, with some rows between ranges.
insert into t3 values ('c-1013=z', 'c-1013=z', 'err');
insert into t3 values ('a-1014=w', 'a-1014=w', 'err');

select a,filler from t3 where (a>='c-1011=w' and a <= 'c-1013=w') or
                              (a>='c-1014=w' and a <= 'c-1015=w');
delete from t3 where b in ('c-1013=z', 'a-1014=w');

# 4. Split is within the equality range.
select a,filler from t3 where a='c-1011=w' or a='c-1012=w' or a='c-1013=w' or
                              a='c-1014=w' or a='c-1015=w';

# 5. Split is at the edge of equality range.
insert into t3 values ('c-1013=w', 'del-me', 'inserted');
select a,filler from t3 where a='c-1011=w' or a='c-1012=w' or a='c-1013=w' or
                              a='c-1014=w' or a='c-1015=w';
delete from t3 where b='del-me';

# PK tests are not included here.

alter table t3 add primary key(b);

##  PK scan tests
# 6. Split is between 'unique' PK ranges
select b,filler from t3 where (b>='c-1011=w' and b<= 'c-1018=w') or 
                              b IN ('c-1019=w', 'c-1020=w', 'c-1021=w', 
                                    'c-1022=w', 'c-1023=w', 'c-1024=w');

# 7. Between non-uniq and uniq range
select b,filler from t3 where (b>='c-1011=w' and b<= 'c-1020=w') or 
                              b IN ('c-1021=w', 'c-1022=w', 'c-1023=w');

# 8. Between uniq and non-uniq range
select b,filler from t3 where (b>='c-1011=w' and b<= 'c-1018=w') or 
                              b IN ('c-1019=w', 'c-1020=w') or 
                              (b>='c-1021=w' and b<= 'c-1023=w');
## End of PK scan tests

#
# Now try different keypart types and special values
#
--disable_warnings
drop table if exists t4;
--enable_warnings
create table t4 (a varchar(10), b int, c char(10), filler char(200),
                 key idx1 (a, b, c));

# insert buffer_size * 1.5 all-NULL tuples
insert into t4 (filler) select concat('NULL-', 15-a) from t2 order by a limit 15;

insert into t4 (a,b,c,filler) 
  select 'b-1',NULL,'c-1', concat('NULL-', 15-a) from t2 order by a limit 15;
insert into t4 (a,b,c,filler) 
  select 'b-1',NULL,'c-222', concat('NULL-', 15-a) from t2 order by a limit 15;
insert into t4 (a,b,c,filler) 
  select 'bb-1',NULL,'cc-2', concat('NULL-', 15-a) from t2 order by a limit 15;
insert into t4 (a,b,c,filler) 
  select 'zz-1',NULL,'cc-2', 'filler-data' from t2 order by a limit 500;

explain 
  select * from t4 where a IS NULL and b IS NULL and (c IS NULL or c='no-such-row1'
                                                      or c='no-such-row2');
select * from t4 where a IS NULL and b IS NULL and (c IS NULL or c='no-such-row1'
                                                    or c='no-such-row2');

explain 
  select * from t4 where (a ='b-1' or a='bb-1') and b IS NULL and (c='c-1' or c='cc-2');
select * from t4 where (a ='b-1' or a='bb-1') and b IS NULL and (c='c-1' or c='cc-2');

select * from t4 ignore index(idx1) where (a ='b-1' or a='bb-1') and b IS NULL and (c='c-1' or c='cc-2');
drop table t1, t2, t3, t4;

#
# Check how ICP works with NULLs and partially-covered indexes
#
create table t1 (a int, b int not null,unique key (a,b),index(b));
insert ignore into t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(null,7),(9,9),(8,8),(7,7),(null,9),(null,9),(6,6);
create table t2 like t1;
insert into t2 select * from t1;
alter table t1 modify b blob not null, add c int not null, drop key a, add unique key (a,b(20),c), drop key b, add key (b(10));

select * from t1 where a is null;
select * from t1 where (a is null or a > 0 and a < 3) and b > 7 limit 3;

select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
drop table t1, t2;
