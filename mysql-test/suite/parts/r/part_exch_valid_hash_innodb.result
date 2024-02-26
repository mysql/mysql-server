use test;
call mtr.add_suppression("row in wrong partition.*from REBUILD/REORGANIZED");
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp_r;
DROP TABLE IF EXISTS tp_rvar;
DROP TABLE IF EXISTS tp_rintvar;
DROP TABLE IF EXISTS tp_rdate;
DROP TABLE IF EXISTS tp_rintdate;
DROP TABLE IF EXISTS tsp_r;
DROP TABLE IF EXISTS tsp_rvar;
DROP TABLE IF EXISTS tsp_rvar1;
DROP TABLE IF EXISTS tp_l;
DROP TABLE IF EXISTS tsp_l;
DROP TABLE IF EXISTS tsp_lvar;
DROP TABLE IF EXISTS tsp_ldate;
DROP TABLE IF EXISTS tp_k;
DROP TABLE IF EXISTS tp_kvar;
DROP TABLE IF EXISTS tp_kdate;
DROP TABLE IF EXISTS tp_h;
DROP TABLE IF EXISTS tp_hvar;
DROP TABLE IF EXISTS tp_hdate;
DROP TABLE IF EXISTS tsp_00;
DROP TABLE IF EXISTS tsp_01;
DROP TABLE IF EXISTS tsp_02;
DROP TABLE IF EXISTS tsp_03;
DROP TABLE IF EXISTS tsp_04;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;
CREATE TABLE t_10 (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB;
CREATE TABLE t_100 (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB;
CREATE TABLE t_1000 (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');
INSERT INTO t_1000 VALUES (111, "Hundred elven", '2111-01-01'), (113, "Hundred thirdteen", '2113-01-01'), (115, "Hundred fiveteen", '2115-01-01'), (119, "Hundred nineteen", '2119-01-01');
INSERT INTO t_1000 VALUES (131, "Hundred thirty-one", '2131-01-01'), (133, "Hundred thirty-three", '2133-01-01'), (135, "Hundred thirty-five", '2135-01-01'), (139, "Hundred thirty-nine", '2139-01-01');
INSERT INTO t_1000 VALUES (151, "Hundred fifty-one", '2151-01-01'), (153, "Hundred fifty-three", '2153-01-01'), (155, "Hundred fity-five", '2155-01-01'), (159, "Hundred fifty-nine", '2159-01-01');
INSERT INTO t_1000 VALUES (191, "Hundred ninety-one", '2191-01-01'), (193, "Hundred ninety-three", '2193-01-01'), (195, "Hundred ninety-five", '2195-01-01'), (199, "Hundred ninety-nine", '2199-01-01');
CREATE TABLE t_empty (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB;
CREATE TABLE t_null (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB;
CREATE TABLE tsp_01(a INT,b VARCHAR(25),c DATE,PRIMARY KEY (a))
charset latin1
ENGINE = InnoDB  
AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 1;
CREATE TABLE tsp_02(a INT,b VARCHAR(25),c DATE,PRIMARY KEY (a)) 
charset latin1
ENGINE = InnoDB  
AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 2;
CREATE TABLE tsp_03(a INT,b VARCHAR(25),c DATE,PRIMARY KEY (a)) 
charset latin1
ENGINE = InnoDB  
AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 3;
CREATE TABLE tsp_04(a INT,b VARCHAR(25),c DATE,PRIMARY KEY (a)) 
charset latin1
ENGINE = InnoDB  
AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 4;
CREATE TABLE tsp_00(a INT,b VARCHAR(25),c DATE,PRIMARY KEY (a)) 
charset latin1
ENGINE = InnoDB  
AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 0;
CREATE TABLE tp_r (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (10)  ,
PARTITION p1 VALUES LESS THAN (100)  ,
PARTITION p2 VALUES LESS THAN (1000)  );
CREATE TABLE tsp_r (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB
PARTITION BY RANGE (a)
SUBPARTITION BY HASH(a)
(PARTITION p0 VALUES LESS THAN (10)  
(SUBPARTITION sp00,
SUBPARTITION sp01,
SUBPARTITION sp02,
SUBPARTITION sp03,
SUBPARTITION sp04),
PARTITION p1 VALUES LESS THAN (100)
(SUBPARTITION sp10  ,
SUBPARTITION sp11  ,
SUBPARTITION sp12  ,
SUBPARTITION sp13  ,
SUBPARTITION sp14  ),
PARTITION p2 VALUES LESS THAN (1000)  
(SUBPARTITION sp20,
SUBPARTITION sp21,
SUBPARTITION sp22,
SUBPARTITION sp23,
SUBPARTITION sp24));
INSERT INTO tp_r VALUES (2, "Two", '2002-01-01'), (4, "Four", '2004-01-01'), (6, "Six", '2006-01-01'), (8, "Eight", '2008-01-01');
INSERT INTO tp_r VALUES (12, "twelve", '2012-01-01'), (14, "Fourteen", '2014-01-01'), (16, "Sixteen", '2016-01-01'), (18, "Eightteen", '2018-01-01');
INSERT INTO tp_r VALUES (112, "Hundred twelve", '2112-01-01'), (114, "Hundred fourteen", '2114-01-01'), (116, "Hundred sixteen", '2116-01-01'), (118, "Hundred eightteen", '2118-01-01');
INSERT INTO tp_r VALUES (122, "Hundred twenty-two", '2122-01-01'), (124, "Hundred twenty-four", '2124-01-01'), (126, "Hundred twenty-six", '2126-01-01'), (128, "Hundred twenty-eight", '2128-01-01');
INSERT INTO tp_r VALUES (162, "Hundred sixty-two", '2162-01-01'), (164, "Hundred sixty-four", '2164-01-01'), (166, "Hundred sixty-six", '2166-01-01'), (168, "Hundred sixty-eight", '2168-01-01');
INSERT INTO tp_r VALUES (182, "Hundred eighty-two", '2182-01-01'), (184, "Hundred eighty-four", '2184-01-01'), (186, "Hundred eighty-six", '2186-01-01'), (188, "Hundred eighty-eight", '2188-01-01');
INSERT INTO tsp_r VALUES (2, "Two", '2002-01-01'), (4, "Four", '2004-01-01'), (6, "Six", '2006-01-01'), (8, "Eight", '2008-01-01');
INSERT INTO tsp_r VALUES (12, "twelve", '2012-01-01'), (14, "Fourteen", '2014-01-01'), (16, "Sixteen", '2016-01-01'), (18, "Eightteen", '2018-01-01');
INSERT INTO tsp_r VALUES (112, "Hundred twelve", '2112-01-01'), (114, "Hundred fourteen", '2114-01-01'), (116, "Hundred sixteen", '2116-01-01'), (118, "Hundred eightteen", '2118-01-01');
INSERT INTO tsp_r VALUES (122, "Hundred twenty-two", '2122-01-01'), (124, "Hundred twenty-four", '2124-01-01'), (126, "Hundred twenty-six", '2126-01-01'), (128, "Hundred twenty-eight", '2128-01-01');
INSERT INTO tsp_r VALUES (162, "Hundred sixty-two", '2162-01-01'), (164, "Hundred sixty-four", '2164-01-01'), (166, "Hundred sixty-six", '2166-01-01'), (168, "Hundred sixty-eight", '2168-01-01');
INSERT INTO tsp_r VALUES (182, "Hundred eight-two", '2182-01-01'), (184, "Hundred eighty-four", '2184-01-01'), (186, "Hundred eighty-six", '2186-01-01'), (188, "Hundred eighty-eight", '2188-01-01');
CREATE TABLE tp_rvar(a INT,b VARCHAR(25),c DATE,PRIMARY KEY (b)) 
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE (b)
(PARTITION p0 VALUES LESS THAN ('H')  ,
PARTITION p1 VALUES LESS THAN ('Q')  ,
PARTITION p2 VALUES LESS THAN ('Z')  ,
PARTITION p3 VALUES LESS THAN (MAXVALUE)  )
AS SELECT a, b FROM tp_r;
ERROR HY000: VALUES value for partition 'p0' must have type INT
CREATE TABLE tp_rintvar(a INT,b VARCHAR(25),c DATE,
PRIMARY KEY (a,b))
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE COLUMNS (a,b)
(PARTITION p0 VALUES LESS THAN (10,'HHHHHHHHHHHHHHHHHHHHHHHHH'),
PARTITION p1 VALUES LESS THAN (100,'PPPPPPPPPPPPPPPPPPPPPPPPP'),
PARTITION p2 VALUES LESS THAN (1000,'WWWWWWWWWWWWWWWWWWWWWWWWW'),
PARTITION p3 VALUES LESS THAN (MAXVALUE,MAXVALUE))
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tp_rvar(a INT,b VARCHAR(25),c DATE) 
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE COLUMNS (b)
(PARTITION p0 VALUES LESS THAN ('HHHHHHHHHHHHHHHHHHHHHHHHH'),
PARTITION p1 VALUES LESS THAN ('PPPPPPPPPPPPPPPPPPPPPPPPP'),
PARTITION p2 VALUES LESS THAN ('WWWWWWWWWWWWWWWWWWWWWWWWW'),
PARTITION p3 VALUES LESS THAN (MAXVALUE))
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tp_rintdate(a INT,b VARCHAR(25),c DATE, 
PRIMARY KEY (a,c))
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE COLUMNS (a,c)
(PARTITION p0 VALUES LESS THAN (10,'2010-01-01'),
PARTITION p1 VALUES LESS THAN (100,'2200-01-01'),
PARTITION p2 VALUES LESS THAN (1000,'2300-01-01'),
PARTITION p3 VALUES LESS THAN (MAXVALUE,MAXVALUE))
AS SELECT a, b, c FROM t_10;
INSERT tp_rintdate SELECT * FROM t_100;
INSERT tp_rintdate SELECT * FROM t_1000;
CREATE TABLE tp_rdate(a INT,b VARCHAR(25),c DATE,
PRIMARY KEY (c))
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE COLUMNS (c)
(PARTITION p0 VALUES LESS THAN ('2010-01-01'),
PARTITION p1 VALUES LESS THAN ('2200-01-01'),
PARTITION p2 VALUES LESS THAN ('2300-01-01'),
PARTITION p3 VALUES LESS THAN (MAXVALUE))
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tsp_rvar(a INT,b VARCHAR(25),c DATE) 
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE COLUMNS (b)
SUBPARTITION BY HASH(a)
SUBPARTITIONS 5
(PARTITION p0 VALUES LESS THAN ('HHHHHHHHHHHHHHHHHHHHHHHHH')
(SUBPARTITION sp00,
SUBPARTITION sp01,
SUBPARTITION sp02,
SUBPARTITION sp03,
SUBPARTITION sp04),
PARTITION p1 VALUES LESS THAN ('PPPPPPPPPPPPPPPPPPPPPPPPP')
(SUBPARTITION sp10,
SUBPARTITION sp11,
SUBPARTITION sp12,
SUBPARTITION sp13,
SUBPARTITION sp14),
PARTITION p2 VALUES LESS THAN ('WWWWWWWWWWWWWWWWWWWWWWWWW')
(SUBPARTITION sp20,
SUBPARTITION sp21,
SUBPARTITION sp22,
SUBPARTITION sp23,
SUBPARTITION sp24))
AS SELECT a, b, c FROM tsp_r;
CREATE TABLE tsp_rvar1(a INT,b VARCHAR(25),c DATE) 
charset latin1
ENGINE = InnoDB  
PARTITION BY RANGE COLUMNS (b)
SUBPARTITION BY HASH(a)
SUBPARTITIONS 5
(PARTITION p0 VALUES LESS THAN ('HHHHHHHHHHHHHHHHHHHHHHHHH'),
PARTITION p1 VALUES LESS THAN ('PPPPPPPPPPPPPPPPPPPPPPPPP'),
PARTITION p2 VALUES LESS THAN ('WWWWWWWWWWWWWWWWWWWWWWWWW'))
AS SELECT a, b, c FROM tsp_r;
CREATE TABLE tp_l (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB
PARTITION BY LIST (a)
(PARTITION p0 VALUES IN (0,1,2,3,4,5,6,7,8,9),
PARTITION p1 VALUES IN (10,11,12,13,14,15,16,17,18,19,90,91,92,93,94,95,96,97,98,99),
PARTITION p2 VALUES IN (110,111,112,113,114,115,116,117,118,119,
120,121,122,123,124,125,126,127,128,129,
130,131,132,133,134,135,136,137,138,139,
150,151,152,153,154,155,156,157,158,159,
160,161,162,163,164,165,166,167,168,169,
180,181,182,183,184,185,186,187,188,189,
190,191,192,193,194,195,196,197,198,199))
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tsp_l (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB
PARTITION BY LIST (a)
SUBPARTITION BY HASH(a)
(PARTITION p0 VALUES IN (0,1,2,3,4,5,6,7,8,9)
(SUBPARTITION sp00,
SUBPARTITION sp01,
SUBPARTITION sp02,
SUBPARTITION sp03,
SUBPARTITION sp04),
PARTITION p1 VALUES IN (10,11,12,13,14,15,16,17,18,19,90,91,92,93,94,95,96,97,98,99)
(SUBPARTITION sp10,
SUBPARTITION sp11,
SUBPARTITION sp12,
SUBPARTITION sp13,
SUBPARTITION sp14),
PARTITION p2 VALUES IN (110,111,112,113,114,115,116,117,118,119,
120,121,122,123,124,125,126,127,128,129,
130,131,132,133,134,135,136,137,138,139,
150,151,152,153,154,155,156,157,158,159,
160,161,162,163,164,165,166,167,168,169,
180,181,182,183,184,185,186,187,188,189,
190,191,192,193,194,195,196,197,198,199)
(SUBPARTITION sp20,
SUBPARTITION sp21,
SUBPARTITION sp22,
SUBPARTITION sp23,
SUBPARTITION sp24))
AS SELECT a, b, c FROM tsp_r;
CREATE TABLE tp_k (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB
PARTITION BY KEY (a)
PARTITIONS 3
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tp_kvar (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (b))  
charset latin1
ENGINE = InnoDB
PARTITION BY KEY (b)
PARTITIONS 3
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tp_kdate (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (c))  
charset latin1
ENGINE = InnoDB
PARTITION BY KEY (c)
PARTITIONS 3
AS SELECT a, b, c FROM tp_r;
CREATE TABLE tp_h (a INT,
b VARCHAR(25),
c DATE,
PRIMARY KEY (a))  
charset latin1
ENGINE = InnoDB
PARTITION BY HASH (a)
PARTITIONS 3
AS SELECT a, b, c FROM tp_r;
####################################################################
### Hash partitioned tables with/out validation
ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
ERROR HY000: Found a row that does not match the partition
SELECT * FROM tp_h PARTITION (p0);
a	b	c
114	Hundred fourteen	2114-01-01
12	twelve	2012-01-01
126	Hundred twenty-six	2126-01-01
162	Hundred sixty-two	2162-01-01
168	Hundred sixty-eight	2168-01-01
18	Eightteen	2018-01-01
186	Hundred eighty-six	2186-01-01
6	Six	2006-01-01
SELECT * FROM tp_h PARTITION (p1);
a	b	c
112	Hundred twelve	2112-01-01
118	Hundred eightteen	2118-01-01
124	Hundred twenty-four	2124-01-01
16	Sixteen	2016-01-01
166	Hundred sixty-six	2166-01-01
184	Hundred eighty-four	2184-01-01
4	Four	2004-01-01
SELECT * FROM tp_h PARTITION (p2);
a	b	c
116	Hundred sixteen	2116-01-01
122	Hundred twenty-two	2122-01-01
128	Hundred twenty-eight	2128-01-01
14	Fourteen	2014-01-01
164	Hundred sixty-four	2164-01-01
182	Hundred eighty-two	2182-01-01
188	Hundred eighty-eight	2188-01-01
2	Two	2002-01-01
8	Eight	2008-01-01
ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_100 WITH VALIDATION;
ERROR HY000: Found a row that does not match the partition
ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_100 WITHOUT VALIDATION;
SELECT * FROM tp_h PARTITION (p0);
a	b	c
11	Eleven	2011-01-01
13	Thirdteen	2013-01-01
15	Fifeteen	2015-01-01
19	Nineteen	2019-01-01
91	Ninety-one	2091-01-01
93	Ninety-three	2093-01-01
95	Ninety-five	2095-01-01
99	Ninety-nine	2099-01-01
SELECT * FROM tp_h PARTITION (p1);
a	b	c
112	Hundred twelve	2112-01-01
118	Hundred eightteen	2118-01-01
124	Hundred twenty-four	2124-01-01
16	Sixteen	2016-01-01
166	Hundred sixty-six	2166-01-01
184	Hundred eighty-four	2184-01-01
4	Four	2004-01-01
SELECT * FROM tp_h PARTITION (p2);
a	b	c
116	Hundred sixteen	2116-01-01
122	Hundred twenty-two	2122-01-01
128	Hundred twenty-eight	2128-01-01
14	Fourteen	2014-01-01
164	Hundred sixty-four	2164-01-01
182	Hundred eighty-two	2182-01-01
188	Hundred eighty-eight	2188-01-01
2	Two	2002-01-01
8	Eight	2008-01-01
ALTER TABLE tp_h COALESCE PARTITION 1;
SELECT * FROM tp_h PARTITION (p0);
a	b	c
112	Hundred twelve	2112-01-01
116	Hundred sixteen	2116-01-01
118	Hundred eightteen	2118-01-01
122	Hundred twenty-two	2122-01-01
124	Hundred twenty-four	2124-01-01
128	Hundred twenty-eight	2128-01-01
14	Fourteen	2014-01-01
16	Sixteen	2016-01-01
164	Hundred sixty-four	2164-01-01
166	Hundred sixty-six	2166-01-01
182	Hundred eighty-two	2182-01-01
184	Hundred eighty-four	2184-01-01
188	Hundred eighty-eight	2188-01-01
2	Two	2002-01-01
4	Four	2004-01-01
8	Eight	2008-01-01
SELECT * FROM tp_h PARTITION (p1);
a	b	c
11	Eleven	2011-01-01
13	Thirdteen	2013-01-01
15	Fifeteen	2015-01-01
19	Nineteen	2019-01-01
91	Ninety-one	2091-01-01
93	Ninety-three	2093-01-01
95	Ninety-five	2095-01-01
99	Ninety-nine	2099-01-01
ALTER TABLE tp_h ADD PARTITION PARTITIONS 4;
ALTER TABLE tp_h EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
SELECT * FROM tp_h PARTITION (p2);
a	b	c
114	Hundred fourteen	2114-01-01
12	twelve	2012-01-01
126	Hundred twenty-six	2126-01-01
162	Hundred sixty-two	2162-01-01
168	Hundred sixty-eight	2168-01-01
18	Eightteen	2018-01-01
186	Hundred eighty-six	2186-01-01
6	Six	2006-01-01
ALTER TABLE tp_h ANALYZE PARTITION p0;
Table	Op	Msg_type	Msg_text
test.tp_h	analyze	status	OK
ALTER TABLE tp_h OPTIMIZE PARTITION p0;
Table	Op	Msg_type	Msg_text
test.tp_h	optimize	note	Table does not support optimize on partitions. All partitions will be rebuilt and analyzed.
test.tp_h	optimize	status	OK
CREATE TABLE t_11 LIKE t_10;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
SELECT * FROM tp_h PARTITION (p0);
a	b	c
1	One	2001-01-01
116	Hundred sixteen	2116-01-01
122	Hundred twenty-two	2122-01-01
128	Hundred twenty-eight	2128-01-01
14	Fourteen	2014-01-01
164	Hundred sixty-four	2164-01-01
182	Hundred eighty-two	2182-01-01
188	Hundred eighty-eight	2188-01-01
2	Two	2002-01-01
3	Three	2003-01-01
5	Five	2005-01-01
8	Eight	2008-01-01
9	Nine	2009-01-01
DELETE FROM tp_h PARTITION (p1);
SELECT * FROM tp_h PARTITION (p1);
a	b	c
ALTER TABLE tp_h CHECK PARTITION p0 ;
Table	Op	Msg_type	Msg_text
test.tp_h	check	error	Found a misplaced row in part 0 should be in part 1:
 a:1
test.tp_h	check	error	Partition p0 returned error
test.tp_h	check	error	Table upgrade required. Please do "REPAIR TABLE `tp_h`" or dump/reload to fix it!
ALTER TABLE tp_h REPAIR PARTITION p0 ;
Table	Op	Msg_type	Msg_text
test.tp_h	repair	warning	Moved 13 misplaced rows
test.tp_h	repair	status	OK
SELECT * FROM tp_h PARTITION (p0);
a	b	c
SELECT * FROM tp_h PARTITION (p1);
a	b	c
1	One	2001-01-01
SELECT * FROM tp_h PARTITION (p2);
a	b	c
114	Hundred fourteen	2114-01-01
116	Hundred sixteen	2116-01-01
12	twelve	2012-01-01
122	Hundred twenty-two	2122-01-01
126	Hundred twenty-six	2126-01-01
128	Hundred twenty-eight	2128-01-01
14	Fourteen	2014-01-01
162	Hundred sixty-two	2162-01-01
164	Hundred sixty-four	2164-01-01
168	Hundred sixty-eight	2168-01-01
18	Eightteen	2018-01-01
182	Hundred eighty-two	2182-01-01
186	Hundred eighty-six	2186-01-01
188	Hundred eighty-eight	2188-01-01
2	Two	2002-01-01
6	Six	2006-01-01
8	Eight	2008-01-01
ALTER TABLE tp_h COALESCE PARTITION 2;
SELECT * FROM tp_h PARTITION (p0);
a	b	c
112	Hundred twelve	2112-01-01
116	Hundred sixteen	2116-01-01
12	twelve	2012-01-01
124	Hundred twenty-four	2124-01-01
128	Hundred twenty-eight	2128-01-01
16	Sixteen	2016-01-01
164	Hundred sixty-four	2164-01-01
168	Hundred sixty-eight	2168-01-01
184	Hundred eighty-four	2184-01-01
188	Hundred eighty-eight	2188-01-01
4	Four	2004-01-01
8	Eight	2008-01-01
SELECT * FROM tp_h PARTITION (p1);
a	b	c
1	One	2001-01-01
5	Five	2005-01-01
9	Nine	2009-01-01
93	Ninety-three	2093-01-01
SELECT * FROM tp_h PARTITION (p2);
a	b	c
114	Hundred fourteen	2114-01-01
118	Hundred eightteen	2118-01-01
122	Hundred twenty-two	2122-01-01
126	Hundred twenty-six	2126-01-01
14	Fourteen	2014-01-01
162	Hundred sixty-two	2162-01-01
166	Hundred sixty-six	2166-01-01
18	Eightteen	2018-01-01
182	Hundred eighty-two	2182-01-01
186	Hundred eighty-six	2186-01-01
2	Two	2002-01-01
6	Six	2006-01-01
SELECT * FROM tp_h PARTITION (p3);
a	b	c
11	Eleven	2011-01-01
15	Fifeteen	2015-01-01
3	Three	2003-01-01
95	Ninety-five	2095-01-01
99	Ninety-nine	2099-01-01
DROP TABLE IF EXISTS t_11;
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp_r;
DROP TABLE IF EXISTS tp_rvar;
DROP TABLE IF EXISTS tp_rintvar;
DROP TABLE IF EXISTS tp_rdate;
DROP TABLE IF EXISTS tp_rintdate;
DROP TABLE IF EXISTS tsp_r;
DROP TABLE IF EXISTS tsp_rvar;
DROP TABLE IF EXISTS tsp_rvar1;
DROP TABLE IF EXISTS tp_l;
DROP TABLE IF EXISTS tsp_l;
DROP TABLE IF EXISTS tsp_lvar;
DROP TABLE IF EXISTS tsp_ldate;
DROP TABLE IF EXISTS tp_k;
DROP TABLE IF EXISTS tp_kvar;
DROP TABLE IF EXISTS tp_kdate;
DROP TABLE IF EXISTS tp_h;
DROP TABLE IF EXISTS tp_hvar;
DROP TABLE IF EXISTS tp_hdate;
DROP TABLE IF EXISTS tsp_00;
DROP TABLE IF EXISTS tsp_01;
DROP TABLE IF EXISTS tsp_02;
DROP TABLE IF EXISTS tsp_03;
DROP TABLE IF EXISTS tsp_04;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;
