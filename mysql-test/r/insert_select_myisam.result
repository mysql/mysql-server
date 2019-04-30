SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE `t1` (
`numeropost` bigint(20) unsigned NOT NULL default '0',
`icone` tinyint(4) unsigned NOT NULL default '0',
`numreponse` bigint(20) unsigned NOT NULL auto_increment,
`contenu` text NOT NULL,
`pseudo` varchar(50) NOT NULL default '',
`date` datetime NOT NULL default '0000-00-00 00:00:00',
`ip` bigint(11) NOT NULL default '0',
`signature` tinyint(1) unsigned NOT NULL default '0',
PRIMARY KEY  (`numeropost`,`numreponse`)
,KEY `ip` (`ip`),
KEY `date` (`date`),
KEY `pseudo` (`pseudo`),
KEY `numreponse` (`numreponse`)
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TABLE `t2` (
`numeropost` bigint(20) unsigned NOT NULL default '0',
`icone` tinyint(4) unsigned NOT NULL default '0',
`numreponse` bigint(20) unsigned NOT NULL auto_increment,
`contenu` text NOT NULL,
`pseudo` varchar(50) NOT NULL default '',
`date` datetime NOT NULL default '0000-00-00 00:00:00',
`ip` bigint(11) NOT NULL default '0',
`signature` tinyint(1) unsigned NOT NULL default '0',
PRIMARY KEY  (`numeropost`,`numreponse`),
KEY `ip` (`ip`),
KEY `date` (`date`),
KEY `pseudo` (`pseudo`),
KEY `numreponse` (`numreponse`)
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t2
(numeropost,icone,numreponse,contenu,pseudo,date,ip,signature) VALUES
(9,1,56,'test','joce','2001-07-25 13:50:53'
,3649052399,0);
INSERT INTO t1 (numeropost,icone,contenu,pseudo,date,signature,ip)
SELECT 1618,icone,contenu,pseudo,date,signature,ip FROM t2
WHERE numeropost=9 ORDER BY numreponse ASC;
show variables like '%bulk%';
Variable_name	Value
bulk_insert_buffer_size	8388608
INSERT INTO t1 (numeropost,icone,contenu,pseudo,date,signature,ip)
SELECT 1718,icone,contenu,pseudo,date,signature,ip FROM t2
WHERE numeropost=9 ORDER BY numreponse ASC;
DROP TABLE t1,t2;
CREATE TABLE t1(
Month date NOT NULL,
Type tinyint(3) unsigned NOT NULL auto_increment,
Field int(10) unsigned NOT NULL,
Count int(10) unsigned NOT NULL,
UNIQUE KEY Month (Month,Type,Field)
)engine=myisam;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
insert into t1 Values
(20030901, 1, 1, 100),
(20030901, 1, 2, 100),
(20030901, 2, 1, 100),
(20030901, 2, 2, 100),
(20030901, 3, 1, 100);
select * from t1;
Month	Type	Field	Count
2003-09-01	1	1	100
2003-09-01	1	2	100
2003-09-01	2	1	100
2003-09-01	2	2	100
2003-09-01	3	1	100
Select null, Field, Count From t1 Where Month=20030901 and Type=2;
NULL	Field	Count
NULL	1	100
NULL	2	100
create table t2(No int not null, Field int not null, Count int not null);
insert into t2 Select null, Field, Count From t1 Where Month=20030901 and Type=2;
Warnings:
Warning	1048	Column 'No' cannot be null
select * from t2;
No	Field	Count
0	1	100
0	2	100
drop table t1, t2;
CREATE TABLE t1 (
ID           int(11) NOT NULL auto_increment,
NO           int(11) NOT NULL default '0',
SEQ          int(11) NOT NULL default '0',
PRIMARY KEY  (ID),
KEY t1$NO    (SEQ,NO)
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 (SEQ, NO) SELECT "1" AS SEQ, IF(MAX(NO) IS NULL, 0, MAX(NO)) + 1 AS NO FROM t1 WHERE (SEQ = 1);
select SQL_BUFFER_RESULT * from t1 WHERE (SEQ = 1);
ID	NO	SEQ
1	1	1
drop table t1;
#
# Bug#30384: Having SQL_BUFFER_RESULT option in the
#            CREATE .. KEY(..) .. SELECT led to creating corrupted index.
#
create table t1(f1 int);
insert into t1 values(1),(2),(3);
create table t2 (key(f1)) engine=myisam select sql_buffer_result f1 from t1;
check table t2 extended;
Table	Op	Msg_type	Msg_text
test.t2	check	status	OK
drop table t1,t2;
