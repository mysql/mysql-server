#
# Set desired charset_connection and collation_collation
# before including this file.
#

# The next query creates a LONGTEXT column
# using the current character_set_connection
# and collation_connection.

create table t1 select repeat('a',4000) a;
delete from t1;

insert into t1 values ('a'), ('a '), ('a\t');
select collation(a),hex(a) from t1 order by a;
drop table t1;


--echo #
--echo # MDEV-5453 Assertion `src' fails in my_strnxfrm_unicode on GROUP BY MID(..) WITH ROLLUP
--echo #
SELECT @@collation_connection;
CREATE TABLE t1 (i INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1),(2);
SELECT * FROM t1 GROUP BY MID(CURRENT_USER,0) WITH ROLLUP;
SELECT * FROM t1 GROUP BY MID('test',0) WITH ROLLUP; 
DROP TABLE t1;
