#
# Test ALTER DROP FOREIGN KEYS
#
--source connect.inc

if (!$algorithm)
{
  die Need the algortihm to use when testing the ALTER;
}

--echo # Setup
create table parent (
  pk int primary key,
  id1 int unique,
  id2 int unique,
  ref1 int,
  ref2 int,
  constraint fk_p1 foreign key ref1_idx(ref1) references parent (id1),
  foreign key ref2_idx(ref2) references parent (id2)
) engine ndb;

--echo # Verify the foreign keys on parent
--let $ndb_table_name=parent
--source verify_foreign_keys.inc

create table child (
  pk int primary key,
  ref1 int,
  ref2 int,
  constraint fk_c1 foreign key ref1_idx(ref1) references parent (id1),
  foreign key ref2_idx(ref2) references parent (id2)
) engine ndb;

--echo # Verify the foreign keys on child
--let $ndb_table_name=child
--source verify_foreign_keys.inc

--echo # Drop foreign keys one by one and verify the other FKs integrity
eval alter table parent
  drop foreign key fk_p1,
  algorithm = $algorithm;

--echo # Verify the foreign keys on parent and child
--let $ndb_table_name=parent
--source verify_foreign_keys.inc
--let $ndb_table_name=child
--source verify_foreign_keys.inc

eval alter table parent
  drop foreign key parent_fk_1,
  algorithm = $algorithm;

--echo # Verify the foreign keys on parent and child
--let $ndb_table_name=parent
--source verify_foreign_keys.inc
--let $ndb_table_name=child
--source verify_foreign_keys.inc

eval alter table child
  drop foreign key fk_c1,
  algorithm = $algorithm;

--echo # Verify the foreign keys on child
--let $ndb_table_name=child
--source verify_foreign_keys.inc

eval alter table child
  drop foreign key child_fk_1,
  algorithm = $algorithm;

--echo # Verify the foreign keys on child
--let $ndb_table_name=child
--source verify_foreign_keys.inc

--echo # Cleanup
drop table child, parent;

# Reset algorithm
--let $algorithm=

