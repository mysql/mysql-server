#
# Test ALTER ADD FOREIGN KEYS
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
  id3 int unique,
  ref1 int,
  ref2 int,
  ref3 int
) engine ndb;

create table child (
  pk int primary key,
  ref1 int,
  ref2 int
) engine ndb;

--echo # Add self referential foreign keys
--echo # Add one with a user specified constraint name
eval alter table parent
  add constraint fk_p1 foreign key ref1_idx(ref1) references parent (id1),
  algorithm = $algorithm;

--echo # Verify the foreign keys on parent
--let $ndb_table_name=parent
--source verify_foreign_keys.inc

--echo # Add one with generated constraint name
eval alter table parent
  add foreign key ref2_idx(ref2) references parent (id2),
  algorithm = $algorithm;

--echo # Verify the foreign keys on parent
--let $ndb_table_name=parent
--source verify_foreign_keys.inc

--echo # Add foreign keys on child referencing parent
--echo # Add one with a user specified constraint name
eval alter table child
  add constraint fk_c1 foreign key ref1_idx(ref1) references parent (id1),
  algorithm = $algorithm;

--echo # Verify the foreign keys on child
--let $ndb_table_name=child
--source verify_foreign_keys.inc

--echo # Add one with generated constraint name
eval alter table child
  add foreign key ref2_idx(ref2) references parent (id2),
  algorithm = $algorithm;

--echo # Verify the foreign keys on child
--let $ndb_table_name=child
--source verify_foreign_keys.inc

--echo # Add a foreign key to the parent and verify that the
--echo # exisiting foreign key constraints are consistent
eval alter table parent
  add foreign key ref3_idx(ref3) references parent (id3),
  algorithm = $algorithm;

--echo # Verify the foreign keys on parent and child
--let $ndb_table_name=parent
--source verify_foreign_keys.inc
--let $ndb_table_name=child
--source verify_foreign_keys.inc

--echo # Cleanup
drop table child, parent;

# Reset algorithm
--let $algorithm=

