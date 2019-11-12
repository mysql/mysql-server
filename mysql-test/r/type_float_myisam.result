CREATE TABLE t1 (
reckey int unsigned NOT NULL,
recdesc varchar(50) NOT NULL,
PRIMARY KEY  (reckey)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
INSERT INTO t1 VALUES (108, 'Has 108 as key');
INSERT INTO t1 VALUES (109, 'Has 109 as key');
select * from t1 where reckey=108;
reckey	recdesc
108	Has 108 as key
select * from t1 where reckey=1.08E2;
reckey	recdesc
108	Has 108 as key
select * from t1 where reckey=109;
reckey	recdesc
109	Has 109 as key
select * from t1 where reckey=1.09E2;
reckey	recdesc
109	Has 109 as key
drop table t1;
