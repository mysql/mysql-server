use test;
SELECT * FROM t_10;
a	b
1	One
3	Three
5	Five
9	Nine
SELECT * FROM t_100;
a	b
11	Eleven
13	Thirdteen
15	Fifeteen
19	Nineteen
91	Ninety-one
93	Ninety-three
95	Ninety-five
99	Ninety-nine
SELECT * FROM t_1000;
a	b
111	Hundred elven
113	Hundred thirdteen
115	Hundred fiveteen
119	Hundred nineteen
131	Hundred thirty-one
133	Hundred thirty-three
135	Hundred thirty-five
139	Hundred thirty-nine
151	Hundred fifty-one
153	Hundred fifty-three
155	Hundred fity-five
159	Hundred fifty-nine
191	Hundred ninety-one
193	Hundred ninety-three
195	Hundred ninety-five
199	Hundred ninety-nine
SELECT * FROM tp;
a	b
112	Hundred twelve
114	Hundred fourteen
116	Hundred sixteen
118	Hundred eightteen
12	twelve
122	Hundred twenty-two
124	Hundred twenty-four
126	Hundred twenty-six
128	Hundred twenty-eight
14	Fourteen
16	Sixteen
162	Hundred sixty-two
164	Hundred sixty-four
166	Hundred sixty-six
168	Hundred sixty-eight
18	Eightteen
182	Hundred eighty-two
184	Hundred eighty-four
186	Hundred eighty-six
188	Hundred eighty-eight
2	Two
4	Four
6	Six
8	Eight
SELECT * FROM tsp;
a	b
112	Hundred twelve
114	Hundred fourteen
116	Hundred sixteen
118	Hundred eightteen
12	twelve
122	Hundred twenty-two
124	Hundred twenty-four
126	Hundred twenty-six
128	Hundred twenty-eight
14	Fourteen
16	Sixteen
162	Hundred sixty-two
164	Hundred sixty-four
166	Hundred sixty-six
168	Hundred sixty-eight
18	Eightteen
182	Hundred eight-two
184	Hundred eighty-four
186	Hundred eighty-six
188	Hundred eighty-eight
2	Two
4	Four
6	Six
8	Eight
SELECT * FROM tsp_00;
a	b
5	Five
SELECT * FROM tsp_01;
a	b
1	One
SELECT * FROM tsp_02;
a	b
SELECT * FROM tsp_03;
a	b
3	Three
SELECT * FROM tsp_04;
a	b
9	Nine
SELECT TABLE_NAME, ROW_FORMAT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'test'
AND TABLE_NAME IN ('tp', 't_100');
TABLE_NAME	ROW_FORMAT
t_100	Compressed
tp	Dynamic
ALTER TABLE tp EXCHANGE PARTITION p1 WITH TABLE t_100;
ERROR HY000: Non matching attribute 'ROW_FORMAT' between partition and table
ALTER TABLE tp EXCHANGE PARTITION p1 WITH TABLE t_1000;
ERROR HY000: Non matching attribute 'MAX_ROWS' between partition and table
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp;
DROP TABLE IF EXISTS tsp;
DROP TABLE IF EXISTS tsp_00;
DROP TABLE IF EXISTS tsp_01;
DROP TABLE IF EXISTS tsp_02;
DROP TABLE IF EXISTS tsp_03;
DROP TABLE IF EXISTS tsp_04;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;
