#
# WL#6737: Create Workload that will be used to fire queries involving
#	   intrinsic tables.
#
drop table if exists t1, t2;
eval create $wl6737_temp table t1 (a int, b varchar(20));
eval create $wl6737_temp table t2 (a int, b varchar(20));
insert into t1 values(1, 'a'), (2, 'b'), (3, 'c'), (3, 'c'), (4, 'c');
insert into t2 values(2, 'd'), (3, 'e'), (4, 'f'), (4, 'f'), (5, 'e');
#
drop function if exists func1;
delimiter |;
create function func1(x int) returns int deterministic
	begin
		declare z1, z2 int;
		set z1 = x;
		set z2 = z1 + 2;
		return z2;
	end|
delimiter ;|
#
drop table if exists t3;
eval create $wl6737_temp table t3 like t1;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
insert into t3 select 5 + 10000 * rand(), '5' from t3;
#
drop table if exists t4;
eval create $wl6737_temp table t4 (a int, b int, c int, d char, e blob);
insert into t4 values (1, 2, 3, '4', '5');
#
drop table if exists t5;
eval create $wl6737_temp table t5 (a int, b int primary key);
insert into t5 values (1,7), (1,8);
#
drop table if exists t6;
eval create $wl6737_temp table t6 (a int, b int, c int, d char);
insert into t6 values(1, 2, 3, '4');
create index t6_indx1 on t6(a);
#
drop table if exists t7;
eval create $wl6737_temp table t7 (a int, b varchar(20));
insert into t7 values(1, 'a'), (2, 'b'), (3, 'c'), (3, 'c'), (4, 'c');
create index indx7 on t7(b);
#
drop table if exists t8;
eval create $wl6737_temp table t8 (
	pk integer $wl6737_auto_inc, col_int_key integer,
	col_varchar_key varchar(1), primary key (pk),
	key (col_int_key), key (col_varchar_key, col_int_key)) engine = innodb;
insert into t8 values (1, 2, 'v'), (2, 150, 'v');
#
drop table if exists t9, t10;
eval create $wl6737_temp table t9 (
  pk int(11) not null $wl6737_auto_inc,
  col_int_nokey int(11) default null,
  col_int_key int(11) default null,
  col_date_key date default null,
  col_date_nokey date default null,
  primary key (pk),
  key col_int_key (col_int_key),
  key col_date_key (col_date_key),
  key col_varchar_key (col_int_key)
) engine=innodb;
eval create $wl6737_temp table t10 (
  pk int(11) not null $wl6737_auto_inc,
  col_int_nokey int(11) default null,
  col_int_key int(11) default null,
  col_date_key date default null,
  col_date_nokey date default null,
  primary key (pk),
  key col_int_key (col_int_key),
  key col_date_key (col_date_key),
  key col_varchar_key (col_int_key)
) engine=innodb;
#
insert into t9 values
	(1, 2, 4, '2008-12-05', '2008-12-05'),
	(2, 150, 62, '2005-03-27', '2005-03-27'),
	(3, NULL, 7, '2004-04-09', '2004-04-09'),
	(4, 2, 1, '2006-05-13', '2006-05-13'),
	(5, 5, 0, '2001-05-06', '2001-05-06'),
	(6, 3, 7, '2006-03-03', '2006-03-03'),
	(7, 1, 7, '2007-12-28', '2007-12-28'),
	(8, 4, 1, '2004-10-20', '2004-10-20'),
	(9, NULL, 7, '2008-04-09', '2008-04-09'),
	(10, 2, 1, '2005-12-25', '2005-12-25'),
	(11, 6, 5, '1900-01-01', '1900-01-01'),
	(12, 6, 2, NULL, NULL),
	(13, 8, 0, '1900-01-01', '1900-01-01'),
	(14, 2, 1, '2001-01-16', '2001-01-16'),
	(15, 6, 8, '1900-01-01', '1900-01-01'),
	(16, 8, 1, '2001-11-23', '2001-11-23'),
	(17, 3, 1, '2004-11-04', '2004-11-04'),
	(18, 3, 9, '2003-03-12', '2003-03-12'),
	(19, 9, 1, '2002-06-22', '2002-06-22'),
	(20, 6, 5, '2004-10-10', '2004-10-10');
insert into t10 values
	(1, 8, 4, '2002-01-03', '2002-01-03'),
	(2, 3, 5, '2007-07-08', '2007-07-08'),
	(3, 3, 8, '2000-08-02', '2000-08-02'),
	(4, NULL, 4, '2000-03-06', '2000-03-06'),
	(5, 7, 8, '2002-06-05', '2002-06-05'),
	(6, 4, 2, '2009-08-09', '2009-08-09'),
	(7, 7, 9, '2001-03-22', '2001-03-22'),
	(8, 7, 6, '2002-10-08', '2002-10-08'),
	(9, 8, NULL, NULL, NULL),
	(10, 6, NULL, '2007-09-13', '2007-09-13'),
	(11, 3, 48, '2003-12-08', '2003-12-08'),
	(12, 210, 228, '2006-06-01', '2006-06-01'),
	(13, 1, 3, '2001-10-04', '2001-10-04'),
	(14, 2, 5, '2004-11-16', '2004-11-16'),
	(15, 251, 39, NULL, NULL),
	(16, 4, 6, '2005-06-22', '2005-06-22'),
	(17, 4, 8, '2003-04-19', '2003-04-19'),
	(18, 9, 3, '2006-03-23', '2006-03-23'),
	(19, 4, NULL, NULL, NULL),
	(20, NULL, 2, '2008-10-22', '2008-10-22'),
	(21, 4, 6, '2009-04-04', '2009-04-04'),
	(22, NULL, 3, NULL, NULL),
	(23, 1, 1, '2001-07-15', '2001-07-15'),
	(24, 6, 4, '2004-09-25', '2004-09-25'),
	(25, 2, 3, '2001-02-20', '2001-02-20'),
	(26, NULL, 1, '1900-01-01', '1900-01-01'),
	(27, 4, NULL, '2004-08-02', '2004-08-02'),
	(28, 248, 97, '2009-02-26', '2009-02-26'),
	(29, 4, 0, '2002-11-23', '2002-11-23'),
	(30, 8, 0, '2008-12-17', '2008-12-17'),
	(31, 4, 9, '2005-08-26', '2005-08-26'),
	(32, 5, 5, '2001-03-10', '2001-03-10'),
	(33, 9, 9, '1900-01-01', '1900-01-01'),
	(34, 2, 0, '2003-08-04', '2003-08-04'),
	(35, 4, 2, '2007-11-06', '2007-11-06'),
	(36, 211, 172, '2009-04-23', '2009-04-23'),
	(37, 2, NULL, '2002-10-06', '2002-10-06'),
	(38, 4, 5, '2008-02-12', '2008-02-12'),
	(39, 125, 119, '2007-09-18', '2007-09-18'),
	(40, 9, 1, '2007-06-26', '2007-06-26'),
	(41, 4, 4, NULL, NULL),
	(42, 8, 8, '2009-05-05', '2009-05-05'),
	(43, 8, NULL, '2003-05-04', '2003-05-04'),
	(44, NULL, 6, '2003-11-10', '2003-11-10'),
	(45, 8, 5, '2009-02-19', '2009-02-19'),
	(46, 4, 5, '2006-11-17', '2006-11-17'),
	(47, 8, 1, '2000-02-23', '2000-02-23'),
	(48, NULL, 7, '1900-01-01', '1900-01-01'),
	(49, 1, 2, '2003-04-08', '2003-04-08'),
	(50, 3, 8, '2006-07-08', '2006-07-08'),
	(51, 5, 9, '2001-05-21', '2001-05-21'),
	(52, 8, NULL, NULL, NULL),
	(53, 7, NULL, '2009-01-05', '2009-01-05'),
	(54, 2, 3, '2001-08-05', '2001-08-05'),
	(55, NULL, 172, '2000-11-01', '2000-11-01'),
	(56, 3, NULL, '2004-12-16', '2004-12-16'),
	(57, NULL, 6, '2003-02-06', '2003-02-06'),
	(58, 9, 6, '2008-04-23', '2008-04-23'),
	(59, 1, 5, '2005-12-20', '2005-12-20'),
	(60, 0, 4, '2002-03-13', '2002-03-13'),
	(61, 0, 3, NULL, NULL),
	(62, 8, 2, '2006-07-20', '2006-07-20'),
	(63, NULL, 7, '2002-12-19', '2002-12-19'),
	(64, 9, 4, '2001-07-09', '2001-07-09'),
	(65, 1, 6, '2006-05-27', '2006-05-27'),
	(66, 9, 0, '2007-02-26', '2007-02-26'),
	(67, 7, 8, NULL, NULL),
	(68, 2, 5, '2007-02-07', '2007-02-07'),
	(69, 1, 8, '2005-01-22', '2005-01-22'),
	(70, 9, 2, '2006-04-25', '2006-04-25'),
	(71, 5, 9, '2002-11-13', '2002-11-13'),
	(72, 4, 7, '2007-10-26', '2007-10-26'),
	(73, 6, 5, '2003-06-16', '2003-06-16'),
	(74, 5, 7, '2006-01-18', '2006-01-18'),
	(75, 2, 0, '1900-01-01', '1900-01-01'),
	(76, 4, 4, '2000-09-13', '2000-09-13'),
	(77, 0, 3, '2003-01-26', '2003-01-26'),
	(78, 3, 1, '2002-09-09', '2002-09-09'),
	(79, 0, 0, '2001-09-06', '2001-09-06'),
	(80, 6, 6, '2006-02-23', '2006-02-23'),
	(81, 1, 2, '2004-06-21', '2004-06-21'),
	(82, 9, NULL, '2006-12-02', '2006-12-02'),
	(83, 4, 8, '2005-05-17', '2005-05-17'),
	(84, 9, NULL, '2001-12-27', '2001-12-27'),
	(85, 4, NULL, '2008-04-11', '2008-04-11'),
	(86, 1, NULL, '2000-09-24', '2000-09-24'),
	(87, 2, 3, '2004-04-20', '2004-04-20'),
	(88, 8, 7, '2008-07-10', '2008-07-10'),
	(89, 1, 3, '2007-08-14', '2007-08-14'),
	(90, 0, 5, '2008-06-08', '2008-06-08'),
	(91, 2, 5, '2001-07-26', '2001-07-26'),
	(92, 0, 1, '2008-09-17', '2008-09-17'),
	(93, 1, 2, NULL, NULL),
	(94, 2, 1, '2004-02-25', '2004-02-25'),
	(95, NULL, 7, '2009-11-02', '2009-11-02'),
	(96, 3, 1, '2001-12-14', '2001-12-14'),
	(97, 8, 9, NULL, NULL),
	(98, 4, 9, '1900-01-01', '1900-01-01'),
	(99, 4, 8, '2002-04-12', '2002-04-12'),
	(100, NULL, 3, '2000-02-09', '2000-02-09');
#
eval create $wl6737_temp table t11 (
  col_date_key date default null,
  col_varchar_10_utf8 varchar(10) CHARACTER SET utf8 default null,
  col_varchar_10_latin1 varchar(10) default null,
  col_varchar_10_latin1_key varchar(10) default null,
  col_datetime datetime default null,
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8 default null,
  col_varchar_1024_latin1_key varchar(1024) default null,
  col_varchar_1024_latin1 varchar(1024) default null,
  col_varchar_1024_utf8 varchar(1024) CHARACTER SET utf8 default null,
  col_int_key int(11) default null,
  col_date date default null,
  col_varchar_1024_utf8_key varchar(1024) CHARACTER SET utf8 default null,
  col_int int(11) default null,
  pk int(11) not null auto_increment,
  col_datetime_key datetime default null,
  primary key (pk),
  key col_date_key (col_date_key),
  key col_varchar_10_latin1_key (col_varchar_10_latin1_key),
  key col_varchar_10_utf8_key (col_varchar_10_utf8_key),
  key col_varchar_1024_latin1_key (col_varchar_1024_latin1_key(1000)),
  key col_int_key (col_int_key),
  key col_varchar_1024_utf8_key (col_varchar_1024_utf8_key(333)),
  key col_datetime_key (col_datetime_key)
) engine=myisam auto_increment=76 default charset=latin1;
#
--disable_warnings
insert ignore into t11 values
	('2005-03-10','have','o','gryvuwdllyhymuwyypoiuyeqbnaanbur','2004','going','h','well','f',1140785152,'2003','YTLDF',NULL,NULL,'20050908213850.042201'),
	('2003', 'g', 'ELTFL', 'got', '20:07:16.048352', 'ryvuwdllyhymuwyypoiuyeqbnaanbursgddnqhyvxnnpadsjiqdkajofferjfslatroorycttbiuhlubvmoikwwnalqyewgthdmfvsedatazrflwenzeiwdxtheqppbmqfydsawvanehgvfvbsqxzkqzhtjkybvikmpexipoddmoulsnzfchusfgkdazecevrpuakfioamyyqyribcnydfxmmzsxcpkydyczmeajaebsaskneaqwxjbpjptiwgiizoxfygnbjojpifpzldsdhrukdbwpzribxgafunywmmcfpjugozduylotsugqocbadhcnxjqlugncbebwsbobhxgghyyphxfqqnpliazusgjswynfotwdonpbkllfdbuoqceirksegypasuuxnrjumrkaryhkgkbdfgoqbdmrdrmpvwitqzswgossxltimkbtrxitnmigcxgfaqmrnvspyaftisgdbkzlevczyrlossygtnatwcobbtsxqhjrdykmthpyzmdgyirxwlidiknxwsufkyexucekyhivscjdcouswuiyltrpmrngriwvrxgovfyewdsvlwirfzkwexxzoccufvuvhsjfvuwjsrrzguntudxxhblzbescayrbbrijnvucxxmbyltlojlgcweddzrfxsmwnxxewigapgrowtmrlqsknuaegzfvdwfdnnbfyubwckwfdrwmrymagyswwvvyeqhaaskgxogthhrzsdlsyqoeuvhwxquwbnivpowtybzehvbsoppuefqngkqohtdeylosjyvtxuziotnfpbqxkpxvzcjbgxokjzitakfevxduhtudsslluhzgcwgrcrtpnczgxchnmmgnubjzvvuklklfhiupbsjzhcqyadhskxtjzabzesulhgpykga', 'she', 'have', 'PYJKD', 9, '2008-06-10', 'yvuwdllyhymuwyypoiuyeqbnaanbursgddnqhyvxnnpadsjiqdkajofferjfslatroorycttbiuhlubvmoikwwnalqyewgthdmfvsedatazrflwenzeiwdxtheqppbmqfydsawvanehgvfvbsqxzkqzhtjkybvikmpexipoddmoulsnzfchusfgkdazecevrpuakfioamyyqyribcnydfxmmzsxcpkydyczmeajaebsaskneaqwxjbpjptiwgiizoxfygnbjojpifpzldsdhrukdbwpzribxgafunywmmcfpjugozduylotsugqocbadhcnxjqlugncbebwsbobhxgghyyphxfqqnpliazusgjswynfotwdonpbkllfdbuoqceirksegypasuuxnrjumrkaryhkgkbdfgoqbdmrdrmpvwitqzswgossxltimkbtrxitnmigcxgfaqmrnv', NULL, NULL, '2008-01-09');
--enable_warnings
#
eval create $wl6737_temp table t12 (t text, c char(10), b blob, d varbinary(10)) engine=innodb;
insert into t12 values (NULL, NULL, NULL,NULL);
insert into t12 values ("", "", "", "");
insert into t12 values ("hello", "hello", "hello", "hello");
insert into t12 values ("HELLO", "HELLO","HELLO", "HELLO");
insert into t12 values ("HELLO MY", "HELLO MY", "HELLO MY", "HELLO MY");
insert into t12 values ("a", "a", "a", "a");
insert into t12 values (1,1,1,1);
insert into t12 values (NULL,NULL,NULL,NULL);
#
#
eval create $wl6737_temp table t13 (
  pk int(11) not null auto_increment,
  col_int_nokey int(11) default null,
  col_int_key int(11) default null,
  col_date_key date default null,
  col_time_key time default null,
  col_varchar_key varchar(1) default null,
  col_varchar_nokey varchar(1) default null,
  primary key (pk),
  key col_int_key (col_int_key),
  key col_date_key (col_date_key),
  key col_time_key (col_time_key),
  key col_varchar_key (col_varchar_key, col_int_key)
) engine=myisam auto_increment=21;
#
insert into t13 values
	(2, 150, 62, '2005-03-27', '14:26:02', 'v', 'v'),
	(3, NULL, 7, '2004-04-09', '14:03:03', 'c', 'c'),
	(4, 2, 1, '2006-05-13', '01:46:09', NULL, NULL),
	(5, 5, 0, '2001-05-06', '16:21:18', 'x', 'x'),
	(6, 3, 7, '2006-03-03', '18:56:33', 'i', 'i'),
	(7, 1, 7, '2007-12-28', NULL, 'e', 'e'),
	(8, 4, 1, '2004-10-20', '09:29:08', 'p', 'p'),
	(9, NULL, 7, '2008-04-09', '19:11:10', 's', 's'),
	(10, 2, 1, '2005-12-25', '11:57:26', 'j', 'j'),
	(11, 6, 5, '1900-01-01', '00:39:46', 'z', 'z'),
	(12, 6, 2, NULL, '03:28:15', 'c', 'c'),
	(13, 8, 0, '1900-01-01', '06:44:18', 'a', 'a'),
	(14, 2, 1, '2001-01-16', '14:36:39', 'q', 'q'),
	(15, 6, 8, '1900-01-01', '18:42:45', 'y', 'y'),
	(16, 8, 1, '2001-11-23', '02:57:29', NULL, NULL),
	(17, 3, 1, '2004-11-04', '16:46:13', 'r', 'r'),
	(18, 3, 9, '2003-03-12', '19:39:02', 'v', 'v'),
	(19, 9, 1, '2002-06-22', NULL, NULL, NULL),
	(20, 6, 5, '2004-10-10', '20:58:33', 'r', 'r');
#
#
eval create $wl6737_temp table t14 (
  pk int(11) not null auto_increment,
  col_int_nokey int(11) default null,
  col_int_key int(11) default null,
  col_date_key date default null,
  col_time_key time default null,
  col_varchar_key varchar(1) default null,
  col_varchar_nokey varchar(1) default null,
  primary key (pk),
  key col_int_key (col_int_key),
  key col_date_key (col_date_key),
  key col_time_key (col_time_key),
  key col_varchar_key (col_varchar_key,col_int_key)
) ENGINE=myisam auto_increment=30;
#
insert into t14 values
	(10, NULL, 8, '2000-12-03', '22:55:23', 'x', 'x'),
	(11, 8, 7, '2008-05-03', '10:19:31', 'd', 'd'),
	(12, 1, 1, '2005-12-06', '14:40:36', 'r', 'r'),
	(13, 9, 7, '2000-04-10', '04:37:47', 'f', 'f'),
	(14, 4, 9, '2002-11-05', '19:34:06', 'y', 'y'),
	(15, 3, NULL, '2000-09-06', '20:35:33', 'u', 'u'),
	(16, 2, 1, NULL, NULL, 'm', 'm'),
	(17, NULL, 9, '2007-06-14', '14:43:37', NULL, NULL),
	(18, 2, 2, '2007-11-17', '02:23:09', 'o', 'o'),
	(19, NULL, 9, '2009-02-23', '01:22:45', 'w', 'w'),
	(20, 6, 2, '2007-01-08', '00:00:00', 'm', 'm'),
	(21, 7, 4, '2008-06-10', '00:13:25', 'q', 'q'),
	(22, 2, 0, '2002-10-20', '03:47:16', NULL, NULL),
	(23, 5, 4, '2008-09-12', '01:41:48', 'd', 'd'),
	(24, 7, 8, '2006-06-16', '00:00:00', 'g', 'g'),
	(25, 6, NULL, '2004-09-18', '22:32:04', 'x', 'x'),
	(26, 6, NULL, '1900-01-01', '16:44:14', 'f', 'f'),
	(27, 2, 0, '2005-09-13', '17:38:37', 'p', 'p'),
	(28, 9, NULL, '2007-04-09', '08:46:48', 'j', 'j'),
	(29, 6, 8, '2000-09-20', '14:11:27', 'c', 'c');

#
# to make stats consistent
analyze table t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14;

