
#
# Spatial objects with keys
#

#
# Bug #30825: Problems when putting a non-spatial index on a GIS column
#

CREATE TABLE t1 (p POINT);
CREATE TABLE t2 (p POINT, INDEX(p));
INSERT INTO t1 VALUES (ST_POINTFROMTEXT('POINT(1 2)'));
INSERT INTO t2 VALUES (ST_POINTFROMTEXT('POINT(1 2)'));

# no index, returns 1 as expected
SELECT COUNT(*) FROM t1 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');

# with index, returns 1 as expected
# EXPLAIN shows that the index is not used though
# due to the "most rows covered anyway, so a scan is more effective" rule
EXPLAIN 
SELECT COUNT(*) FROM t2 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');
SELECT COUNT(*) FROM t2 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');

# adding another row to the table so that
# the "most rows covered" rule doesn't kick in anymore
# now EXPLAIN shows the index used on the table
# and we're getting the wrong result again
INSERT INTO t1 VALUES (ST_POINTFROMTEXT('POINT(1 2)'));
INSERT INTO t2 VALUES (ST_POINTFROMTEXT('POINT(1 2)'));
EXPLAIN 
SELECT COUNT(*) FROM t1 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');
SELECT COUNT(*) FROM t1 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');

EXPLAIN 
SELECT COUNT(*) FROM t2 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');
SELECT COUNT(*) FROM t2 WHERE p=ST_POINTFROMTEXT('POINT(1 2)');

EXPLAIN 
SELECT COUNT(*) FROM t2 IGNORE INDEX(p) WHERE p=ST_POINTFROMTEXT('POINT(1 2)');
SELECT COUNT(*) FROM t2 IGNORE INDEX(p) WHERE p=ST_POINTFROMTEXT('POINT(1 2)');

DROP TABLE t1, t2;

--echo End of 5.0 tests


--echo #
--echo # Test for bug #58650 "Failing assertion: primary_key_no == -1 ||
--echo #                      primary_key_no == 0".
--echo #
--disable_warnings
drop table if exists t1;
--enable_warnings
--echo # The minimal test case.
create table t1 (a int not null, b linestring not null, unique key b (b(12)), unique key a (a));
drop table t1;
--echo # The original test case.
create table t1 (a int not null, b linestring not null, unique key b (b(12)));
create unique index a on t1(a);
drop table t1;
