--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# In MySQL 4.0.4 doing a GROUP BY on a NULL column created a disk based
# temporary table when a memory based one would be good enough.

CREATE TABLE t1 (
  d datetime default NULL
) ENGINE=MyISAM;


INSERT INTO t1 VALUES ('2002-10-24 14:50:32'),('2002-10-24 14:50:33'),('2002-10-24 14:50:34'),('2002-10-24 14:50:34'),('2002-10-24 14:50:34'),('2002-10-24 14:50:35'),('2002-10-24 14:50:35'),('2002-10-24 14:50:35'),('2002-10-24 14:50:35'),('2002-10-24 14:50:36'),('2002-10-24 14:50:36'),('2002-10-24 14:50:36'),('2002-10-24 14:50:36'),('2002-10-24 14:50:37'),('2002-10-24 14:50:37'),('2002-10-24 14:50:37'),('2002-10-24 14:50:37'),('2002-10-24 14:50:38'),('2002-10-24 14:50:38'),('2002-10-24 14:50:38'),('2002-10-24 14:50:39'),('2002-10-24 14:50:39'),('2002-10-24 14:50:39'),('2002-10-24 14:50:39'),('2002-10-24 14:50:40'),('2002-10-24 14:50:40'),('2002-10-24 14:50:40');

flush status;
select * from t1 group by d;
--skip_if_hypergraph  # Depends on the query plan.
show status like "created_tmp%tables";
drop table t1;

# Test for admin statements supported only by MyISAM

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
CREATE TEMPORARY TABLE t1(a INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1), (2), (3);
OPTIMIZE TABLE t1;
INSERT INTO t1 VALUES (1), (2), (3);
REPAIR TABLE t1;
DROP TABLES t1;
