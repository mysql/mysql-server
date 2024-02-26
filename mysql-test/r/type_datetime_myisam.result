SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE `t1` (
`date` datetime NOT NULL default '0000-00-00 00:00:00',
`numfacture` int(6) unsigned NOT NULL default '0',
`expedition` datetime NOT NULL default '0000-00-00 00:00:00',
PRIMARY KEY  (`numfacture`),
KEY `date` (`date`),
KEY `expedition` (`expedition`)
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 (expedition) VALUES ('0001-00-00 00:00:00');
SELECT * FROM t1 WHERE expedition='0001-00-00 00:00:00';
date	numfacture	expedition
0000-00-00 00:00:00	0	0001-00-00 00:00:00
INSERT INTO t1 (numfacture,expedition) VALUES ('1212','0001-00-00 00:00:00');
SELECT * FROM t1 WHERE expedition='0001-00-00 00:00:00';
date	numfacture	expedition
0000-00-00 00:00:00	0	0001-00-00 00:00:00
0000-00-00 00:00:00	1212	0001-00-00 00:00:00
EXPLAIN SELECT * FROM t1 WHERE expedition='0001-00-00 00:00:00';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ref	expedition	expedition	5	const	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`date` AS `date`,`test`.`t1`.`numfacture` AS `numfacture`,`test`.`t1`.`expedition` AS `expedition` from `test`.`t1` where (`test`.`t1`.`expedition` = TIMESTAMP'0001-00-00 00:00:00')
drop table t1;
create table t1 (a datetime not null, b datetime not null);
insert into t1 values (now(), now());
insert into t1 values (now(), now());
select * from t1 where a is null or b is null;
a	b
drop table t1;
