DROP TABLE IF EXISTS t1;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (
cont_nr int(11) NOT NULL auto_increment,
ver_nr int(11) NOT NULL default '0',
aufnr int(11) NOT NULL default '0',
username varchar(50) NOT NULL default '',
hdl_nr int(11) NOT NULL default '0',
eintrag date NOT NULL default '0000-00-00',
st_klasse varchar(40) NOT NULL default '',
st_wert varchar(40) NOT NULL default '',
st_zusatz varchar(40) NOT NULL default '',
st_bemerkung varchar(255) NOT NULL default '',
kunden_art varchar(40) NOT NULL default '',
mcbs_knr int(11) default NULL,
mcbs_aufnr int(11) NOT NULL default '0',
schufa_status char(1) default '?',
bemerkung text,
wirknetz text,
wf_igz int(11) NOT NULL default '0',
tarifcode varchar(80) default NULL,
recycle char(1) default NULL,
sim varchar(30) default NULL,
mcbs_tpl varchar(30) default NULL,
emp_nr int(11) NOT NULL default '0',
laufzeit int(11) default NULL,
hdl_name varchar(30) default NULL,
prov_hdl_nr int(11) NOT NULL default '0',
auto_wirknetz varchar(50) default NULL,
auto_billing varchar(50) default NULL,
touch timestamp NOT NULL,
kategorie varchar(50) default NULL,
kundentyp varchar(20) NOT NULL default '',
sammel_rech_msisdn varchar(30) NOT NULL default '',
p_nr varchar(9) NOT NULL default '',
suffix char(3) NOT NULL default '',
PRIMARY KEY (cont_nr),
KEY idx_aufnr(aufnr),
KEY idx_hdl_nr(hdl_nr),
KEY idx_st_klasse(st_klasse),
KEY ver_nr(ver_nr),
KEY eintrag_idx(eintrag),
KEY emp_nr_idx(emp_nr),
KEY wf_igz(wf_igz),
KEY touch(touch),
KEY hdl_tag(eintrag,hdl_nr),
KEY prov_hdl_nr(prov_hdl_nr),
KEY mcbs_aufnr(mcbs_aufnr),
KEY kundentyp(kundentyp),
KEY p_nr(p_nr,suffix)
);
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 VALUES (3359356,405,3359356,'Mustermann Musterfrau',52500,'2000-05-20','workflow','Auftrag erledigt','Originalvertrag eingegangen und geprüft','','privat',1485525,2122316,'+','','N',1909160,'MobilComSuper92000D2',NULL,NULL,'MS9ND2',3,24,'MobilCom Shop Koeln',52500,NULL,'auto',20010202105916,'Mobilfunk','PP','','','');
INSERT INTO t1 VALUES (3359357,468,3359357,'Mustermann Musterfrau',7001,'2000-05-20','workflow','Auftrag erledigt','Originalvertrag eingegangen und geprüft','','privat',1503580,2139699,'+','','P',1909171,'MobilComSuper9D1T10SFreisprech(Akquise)',NULL,NULL,'MS9NS1',327,24,'MobilCom Intern',7003,NULL,'auto',20010202105916,'Mobilfunk','PP','','','');
INSERT INTO t1 VALUES (3359358,407,3359358,'Mustermann Musterfrau',7001,'2000-05-20','workflow','Auftrag erledigt','Originalvertrag eingegangen und geprüft','','privat',1501358,2137473,'N','','N',1909159,'MobilComSuper92000D2',NULL,NULL,'MS9ND2',325,24,'MobilCom Intern',7003,NULL,'auto',20010202105916,'Mobilfunk','PP','','','');
INSERT INTO t1 VALUES (3359359,468,3359359,'Mustermann Musterfrau',7001,'2000-05-20','workflow','Auftrag erledigt','Originalvertrag eingegangen und geprüft','','privat',1507831,2143894,'+','','P',1909162,'MobilComSuper9D1T10SFreisprech(Akquise)',NULL,NULL,'MS9NS1',327,24,'MobilCom Intern',7003,NULL,'auto',20010202105916,'Mobilfunk','PP','','','');
INSERT INTO t1 VALUES (3359360,0,0,'Mustermann Musterfrau',29674907,'2000-05-20','workflow','Auftrag erledigt','Originalvertrag eingegangen und geprüft','','privat',1900169997,2414578,'+',NULL,'N',1909148,'',NULL,NULL,'RV99066_2',20,NULL,'POS',29674907,NULL,NULL,20010202105916,'Mobilfunk','','','97317481','007');
INSERT INTO t1 VALUES (3359361,406,3359361,'Mustermann Musterfrau',7001,'2000-05-20','workflow','Auftrag storniert','','(7001-84):Storno, Kd. möchte nicht mehr','privat',NULL,0,'+','','P',1909150,'MobilComSuper92000D1(Akquise)',NULL,NULL,'MS9ND1',325,24,'MobilCom Intern',7003,NULL,'auto',20010202105916,'Mobilfunk','PP','','','');
INSERT INTO t1 VALUES (3359362,406,3359362,'Mustermann Musterfrau',7001,'2000-05-20','workflow','Auftrag erledigt','Originalvertrag eingegangen und geprüft','','privat',1509984,2145874,'+','','P',1909154,'MobilComSuper92000D1(Akquise)',NULL,NULL,'MS9ND1',327,24,'MobilCom Intern',7003,NULL,'auto',20010202105916,'Mobilfunk','PP','','','');
SELECT ELT(FIELD(kundentyp,'PP','PPA','PG','PGA','FK','FKA','FP','FPA','K','KA','V','VA',''), 'Privat (Private Nutzung)','Privat (Private Nutzung) Sitz im Ausland','Privat (geschaeftliche Nutzung)','Privat (geschaeftliche Nutzung) Sitz im Ausland','Firma (Kapitalgesellschaft)','Firma (Kapitalgesellschaft) Sitz im Ausland','Firma (Personengesellschaft)','Firma (Personengesellschaft) Sitz im Ausland','oeff. rechtl. Koerperschaft','oeff. rechtl. Koerperschaft Sitz im Ausland','Eingetragener Verein','Eingetragener Verein Sitz im Ausland','Typ unbekannt') AS Kundentyp ,kategorie FROM t1 WHERE hdl_nr < 2000000 AND kategorie IN ('Prepaid','Mobilfunk') AND st_klasse = 'Workflow' GROUP BY kundentyp ORDER BY kategorie;
Kundentyp	kategorie
Privat (Private Nutzung)	Mobilfunk
Warnings:
Warning	1052	Column 'kundentyp' in group statement is ambiguous
drop table t1;
CREATE TABLE t1 (
AUFNR varchar(12) NOT NULL default '',
PLNFL varchar(6) NOT NULL default '',
VORNR varchar(4) NOT NULL default '',
xstatus_vor smallint(5) unsigned NOT NULL default '0'
);
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 VALUES ('40004712','000001','0010',9);
INSERT INTO t1 VALUES ('40004712','000001','0020',0);
UPDATE t1 SET t1.xstatus_vor = Greatest(t1.xstatus_vor,1) WHERE t1.aufnr =
"40004712" AND t1.plnfl = "000001" AND t1.vornr > "0010" ORDER BY t1.vornr
ASC LIMIT 1;
drop table t1;
drop table if exists t1,t2,t3;
create table t1 (a int, b int, c int);
create table t2 (d int);
create table t3 (a1 int, b1 int, c1 int);
insert into t1 values(1,2,3);
insert into t1 values(11,22,33);
insert into t2 values(99);
select t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
select t2.* as 'with_alias' from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t2' at line 1
select t1.*, t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
select t1.* as 'with_alias', t1.* from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', t1.* from t1' at line 1
select t1.* as 'with_alias', t1.* as 'alias2' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', t1.* as 'alias2' from t1' at line 1
select t1.* as 'with_alias', a, t1.* as 'alias2' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', a, t1.* as 'alias2' from t1' at line 1
select a, t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
select t1.* as 'with_alias', a from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', a from t1' at line 1
select a, t1.* as 'with_alias', b from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', b from t1' at line 1
select (select d from t2 where d > a), t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
select t1.* as 'with_alias', (select a from t2 where d > a) from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', (select a from t2 where d > a) from t1' at line 1
select a as 'x', t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
select t1.* as 'with_alias', a as 'x' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', a as 'x' from t1' at line 1
select a as 'x', t1.* as 'with_alias', b as 'x' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', b as 'x' from t1' at line 1
select (select d from t2 where d > a) as 'x', t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
select t1.* as 'with_alias', (select a from t2 where d > a) as 'x' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', (select a from t2 where d > a) as 'x' from t1' at line 1
select (select t2.* as 'x' from t2) from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'x' from t2) from t1' at line 1
select a, (select t2.* as 'x' from t2) from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'x' from t2) from t1' at line 1
select t1.*, (select t2.* as 'x' from t2) from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'x' from t2) from t1' at line 1
insert into t3 select t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
insert into t3 select t2.* as 'with_alias', 1, 2 from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', 1, 2 from t2' at line 1
insert into t3 select t2.* as 'with_alias', d as 'x', d as 'z' from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', d as 'x', d as 'z' from t2' at line 1
insert into t3 select t2.*, t2.* as 'with_alias', 3 from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', 3 from t2' at line 1
create table t3 select t1.* as 'with_alias' from t1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias' from t1' at line 1
create table t3 select t2.* as 'with_alias', 1, 2 from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', 1, 2 from t2' at line 1
create table t3 select t2.* as 'with_alias', d as 'x', d as 'z' from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', d as 'x', d as 'z' from t2' at line 1
create table t3 select t2.*, t2.* as 'with_alias', 3 from t2;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'as 'with_alias', 3 from t2' at line 1
select t1.* from t1;
a	b	c
1	2	3
11	22	33
select t2.* from t2;
d
99
select t1.*, t1.* from t1;
a	b	c	a	b	c
1	2	3	1	2	3
11	22	33	11	22	33
select t1.*, a, t1.* from t1;
a	b	c	a	a	b	c
1	2	3	1	1	2	3
11	22	33	11	11	22	33
select a, t1.* from t1;
a	a	b	c
1	1	2	3
11	11	22	33
select t1.*, a from t1;
a	b	c	a
1	2	3	1
11	22	33	11
select a, t1.*, b from t1;
a	a	b	c	b
1	1	2	3	2
11	11	22	33	22
select (select d from t2 where d > a), t1.* from t1;
(select d from t2 where d > a)	a	b	c
99	1	2	3
99	11	22	33
select t1.*, (select a from t2 where d > a) from t1;
a	b	c	(select a from t2 where d > a)
1	2	3	1
11	22	33	11
select a as 'x', t1.* from t1;
x	a	b	c
1	1	2	3
11	11	22	33
select t1.*, a as 'x' from t1;
a	b	c	x
1	2	3	1
11	22	33	11
select a as 'x', t1.*, b as 'x' from t1;
x	a	b	c	x
1	1	2	3	2
11	11	22	33	22
select (select d from t2 where d > a) as 'x', t1.* from t1;
x	a	b	c
99	1	2	3
99	11	22	33
select t1.*, (select a from t2 where d > a) as 'x' from t1;
a	b	c	x
1	2	3	1
11	22	33	11
select (select t2.* from t2) from t1;
(select t2.* from t2)
99
99
select a, (select t2.* from t2) from t1;
a	(select t2.* from t2)
1	99
11	99
select t1.*, (select t2.* from t2) from t1;
a	b	c	(select t2.* from t2)
1	2	3	99
11	22	33	99
insert into t3 select t1.* from t1;
insert into t3 select t2.*, 1, 2 from t2;
insert into t3 select t2.*, d as 'x', d as 'z' from t2;
insert into t3 select t2.*, t2.*, 3 from t2;
create table t4 select t1.* from t1;
drop table t4;
create table t4 select t2.*, 1, 2 from t2;
drop table t4;
create table t4 select t2.*, d as 'x', d as 'z' from t2;
drop table t4;
drop table t1,t2,t3;
SET sql_mode = default;
