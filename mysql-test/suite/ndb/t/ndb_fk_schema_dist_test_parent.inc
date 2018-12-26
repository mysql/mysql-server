# Inc file to check if parent's metadata is flushed out when child's
# table definition is updated.
# It does the following :
# 1. Cache parent table definition in server1
# 2. Run the given DDL in server2
# 3. Run 'alter .. change column a ..' on parent at server1
#    to check if the cached table has been flushed out
#
# Input :
# $ddl_query            : The DDL query to run in server2.
# $expect_alter_failure : Updating parent's column 'a' in server1 after
#                         running $ddl_query in server2 is expected to
#                         fail if this value is set to 1 and expected to
#                         pass if the value is set to 0.

--echo #cache parent table in server1
--connection server1
select * from parent order by a;

--echo #now run the DDL query in server2
--connection server2
--eval $ddl_query

--echo #check if the parent metadata has been reset in server1
--connection server1
if (!$expect_alter_failure)
{
  --echo #altering column should pass
}
if ($expect_alter_failure)
{
  --echo #altering column should fail with error
  --error ER_FK_COLUMN_CANNOT_CHANGE_CHILD
}
alter table parent
  change column a a varchar(64) not null;

if (!$expect_alter_failure)
{
  --echo #check parent's new definition
  show create table parent;

  --echo #restore parent to old definition
  alter table parent change column a a int;
}

#reset input
let $ddl_query=;
