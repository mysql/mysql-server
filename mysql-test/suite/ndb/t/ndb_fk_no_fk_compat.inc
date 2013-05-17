#
# Test legacy compatibility of no fk foreign key support
#  - snippets of other tests converted to use different engines
#    for quick and easy compatibility test
#

if (!$engine)
{
  die Missing parameter $engine;
}

#
# Test 1
#

SET foreign_key_checks=0;
--replace_result $engine engine
eval CREATE TABLE t1 (id int primary key, foreign key (id) references t2(id)) ENGINE=$engine;
--replace_result $engine engine
eval CREATE TABLE t2 (id int primary key, foreign key (id) references t1(id)) ENGINE=$engine;
SET foreign_key_checks=1;
SELECT COUNT(*) FROM information_schema.key_column_usage
  WHERE REFERENCED_TABLE_NAME in ('t1', 't2');
SET foreign_key_checks=0;
#
# Bug#16817928
# Check dropping in both order
DROP TABLE t1, t2;

SET foreign_key_checks=0;
--replace_result $engine engine
eval CREATE TABLE t1 (id int primary key, foreign key (id) references t2(id)) ENGINE=$engine;
--replace_result $engine engine
eval CREATE TABLE t2 (id int primary key, foreign key (id) references t1(id)) ENGINE=$engine;
SET foreign_key_checks=1;
SELECT COUNT(*) FROM information_schema.key_column_usage
  WHERE REFERENCED_TABLE_NAME in ('t1', 't2');
SET foreign_key_checks=0;
#
# Bug#16817928
# Check dropping in both order
DROP TABLE t2, t1;

#
# Test n
#


