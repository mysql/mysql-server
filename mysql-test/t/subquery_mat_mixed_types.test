--echo #
--echo # Bug#13960580 SUBQUERY MATERIALIZATION IS TOO RESTRICTIVE ON DATA TYPES
--echo #

CREATE TABLE t1 (c1 INT, KEY(c1));
CREATE TABLE t2 (c1 BIGINT, KEY(c1));
CREATE TABLE t3 (c1 DECIMAL(10,2), KEY(c1));
CREATE TABLE t4 (c1 FLOAT, KEY(c1));
CREATE TABLE t5 (c1 DOUBLE, KEY(c1));
CREATE TABLE t6 (c1 CHAR(60), KEY(c1));
CREATE TABLE t7 (c1 VARCHAR(60), KEY(c1));
CREATE TABLE t8 (c1 TIME, KEY(c1));
CREATE TABLE t9 (c1 TIMESTAMP, KEY(c1));
CREATE TABLE t10 (c1 DATE, KEY(c1));
CREATE TABLE t11 (c1 DATETIME, KEY(c1));
CREATE TABLE t12 (c1 CHAR(10) CHARACTER SET UTF16, KEY(c1));
CREATE TABLE t13 (c1 BIGINT UNSIGNED, KEY(c1));

INSERT INTO t1 VALUES (19910113), (20010514), (19930513), (19970416), (19960416),
                      (19950414);
INSERT INTO t2 VALUES (19930513), (19990419), (19950414), (-1), (-19950414);
INSERT INTO t3 VALUES (19930513.3), (19990519), (19950414.0), (19950414.1);
INSERT INTO t4 VALUES (19930513.3), (19990419.2), (19950414e0), (19950414.1e0);
INSERT INTO t5 VALUES (19930513.3), (19990419.2), (19950414e0), (19950414.1e0);
INSERT INTO t6 VALUES ('19910111'), ('20010513'), ('19930513'), ('19950414'),
                      ('19950414.1');
INSERT INTO t7 VALUES ('19910111'), ('20010513'), ('19930513'), ('19950414'),
                      ('19950414.1');
# Note that 33:22:33 means 33 hours from the current date.
INSERT INTO t8 VALUES ('10:22:33'), ('12:34:56'), ('33:22:33');
INSERT INTO t9 VALUES (20150413102233), (19990102123456);
INSERT INTO t10 VALUES ('1998-01-01'), ('2015-04-13');
INSERT INTO t11 VALUES ('1999-08-14 01:00:00'), ('2015-04-13 10:22:33'),
('2015-04-14 09:22:33');
INSERT INTO t12 VALUES ('19910111'), ('19930513'), ('20010513'), ('19950414')
, ('19950414.1');
INSERT INTO t13 VALUES (19950414),(18446744073709551615); # 2^64-1

ANALYZE TABLE t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13;

# Test subquery materialization
set optimizer_switch='semijoin=off,materialization=on,subquery_materialization_cost_based=off';
--disable_warnings

let $c=2;

while ($c)
{
if ($c == 1)
{
# Test semijoin materialization. When semijoin transformation doesn't
# apply (HAVING), between IN-to-EXISTS and subquery materialization we
# force the latter to get repeatability.
set optimizer_switch='semijoin=on,firstmatch=off,loosescan=off,duplicateweedout=off,materialization=on,subquery_materialization_cost_based=off';
}
dec $c;

# Set "today", so TIME is extended with same day as what's in
# DATETIME/TIMESTAMP
SET TIMESTAMP=UNIX_TIMESTAMP(20150413000000);
# Note that results of comparing TIME with DATETIME/TIMESTAMP are not
# consistent between IN-to-EXISTS and materializations, see bug#75644.

# Below we use EXPLAIN to see if materialization is chosen when it
# should. In some cases, duplicates-weedout is chosen, which is
# correct; but then the two tables are ordered in a not fully
# repeatable way (probably because they're similar in size and cost of
# access). All we want to know is if materialization was chosen or not.
# So we eliminate all columns of EXPLAIN but the one which tells if
# materialization was chosen or not ("SUBQUERY" word, and not
# "DEPENDENT SUBQUERY"; "SIMPLE" denotes another strategy).

let $table_count= 13;
let $i= 1;
let $j= 2;
while ($i <= $table_count)
{
    while ($j <= $table_count)
    {
       if ($i != $j)
       {
          let $stmt1= SELECT * FROM t$i STRAIGHT_JOIN t$j WHERE t$i.c1=t$j.c1;
          --replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
          eval EXPLAIN $stmt1;
          --sorted_result
          # The combination c2/c13 on this query hits bug #31832001
          # (HASH JOIN MISMATCHES SIGNED AND UNSIGNED), which is not related
          # to the hypergraph optimizer in itself, only exposed by it.
          --skip_if_hypergraph
          eval $stmt1;
          let $stmt2= SELECT * FROM t$i WHERE c1 IN (SELECT c1 FROM t$j);
          --replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
          eval EXPLAIN $stmt2;
          --sorted_result
          # Same bug as in the previous query.
          --skip_if_hypergraph
          eval $stmt2;
          let $stmt3= SELECT * FROM t$i GROUP BY c1 HAVING c1 IN (SELECT c1 FROM t$j);
          --replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
          eval EXPLAIN $stmt3;
          --sorted_result
          if ($i == 8 && ($j == 9 || $j == 11)) {
                 # This hits bug#75644 (a.k.a bug#20421039).
                 --skip_if_hypergraph
          }
          eval $stmt3;
          if ($i == 11)
          {
                # This value causes two rows in scalar subquery below,
                # so remove it just for this query
                DELETE FROM t11 WHERE c1='2015-04-14 09:22:33';
                ANALYZE TABLE t11;
          }
          let $stmt4= SELECT 1 FROM t$i WHERE (SELECT 1 FROM t$i WHERE ASCII(c1) = 50) IN
                                              (SELECT 1 FROM t$j WHERE ASCII(c1) = 50);
          --replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
          eval EXPLAIN $stmt4;
          --sorted_result
          eval $stmt4;
          if ($i == 11)
          {
                INSERT INTO t11 SET c1='2015-04-14 09:22:33';
                ANALYZE TABLE t11;
          }
       }
       inc $j;
    }
    inc $i;
    let $j=1;
}

# Set "today" differently, so TIME is extended with another day than
# what's in DATETIME/TIMESTAMP
SET TIMESTAMP=UNIX_TIMESTAMP(20140413000000);
# and now some matches should disappear:

let $i=8;
let $j=9;
let $stmt1= SELECT * FROM t$i STRAIGHT_JOIN t$j WHERE t$i.c1=t$j.c1;
--replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
eval EXPLAIN $stmt1;
eval $stmt1;
let $stmt2= SELECT * FROM t$i WHERE c1 IN (SELECT c1 FROM t$j);
--replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
eval EXPLAIN $stmt2;
eval $stmt2;

let $i=9;
let $j=8;
let $stmt1= SELECT * FROM t$i STRAIGHT_JOIN t$j WHERE t$i.c1=t$j.c1;
--replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
eval EXPLAIN $stmt1;
--sorted_result
eval $stmt1;
let $stmt2= SELECT * FROM t$i WHERE c1 IN (SELECT c1 FROM t$j);
--replace_column 1 # 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 #
eval EXPLAIN $stmt2;
--sorted_result
eval $stmt2;


}

--enable_warnings

DROP TABLE t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13;
