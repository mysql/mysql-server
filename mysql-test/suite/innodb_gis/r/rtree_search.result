create table t1 (c1 int, c2 geometry not null SRID 0, spatial index (c2))engine=innodb;
create procedure insert_t1(IN total int)
begin
declare i int default 1;
while (i <= total) DO
insert into t1 values (i, Point(i, i));
set i = i + 1;
end while;
end|
CALL insert_t1(1000);
select count(*) from t1;
count(*)
1000
set @g1 = ST_GeomFromText('Polygon((0 0,0 1000,1000 1000,1000 0,0 0))');
select count(*) from t1 where MBRWithin(t1.c2, @g1);
count(*)
999
set @g1 = ST_GeomFromText('Polygon((0 0,0 100,100 100,100 0,0 0))');
select count(*) from t1 where MBRWithin(t1.c2, @g1);
count(*)
99
set @g1 = ST_GeomFromText('Polygon((10 10,10 800,800 800,800 10,10 10))');
select count(*) from t1 where MBRWithin(t1.c2, @g1);
count(*)
789
set @g1 = ST_GeomFromText('Polygon((100 100,100 800,800 800,800 100,100 100))');
select count(*) from t1 where MBRWithin(t1.c2, @g1);
count(*)
699
set @g1 =  ST_GeomFromText('Point(1 1)');
select count(*) from t1 where MBRequals(t1.c2, @g1);
count(*)
1
set @g1 = ST_GeomFromText('Polygon((0 0,0 100,100 100,100 0,0 0))');
select count(*) from t1 where MBRdisjoint(t1.c2, @g1);
count(*)
900
DROP PROCEDURE insert_t1;
truncate t1;
INSERT INTO t1  VALUES (150, ST_GeomFromText('LineString(150 150, 150 150)'));
INSERT INTO t1  VALUES (149, ST_GeomFromText('LineString(149 149, 151 151)'));
INSERT INTO t1  VALUES (148, ST_GeomFromText('LineString(148 148, 152 152)'));
INSERT INTO t1  VALUES (147, ST_GeomFromText('LineString(147 147, 153 153)'));
INSERT INTO t1  VALUES (146, ST_GeomFromText('LineString(146 146, 154 154)'));
INSERT INTO t1  VALUES (145, ST_GeomFromText('LineString(145 145, 155 155)'));
INSERT INTO t1  VALUES (144, ST_GeomFromText('LineString(144 144, 156 156)'));
INSERT INTO t1  VALUES (143, ST_GeomFromText('LineString(143 143, 157 157)'));
INSERT INTO t1  VALUES (142, ST_GeomFromText('LineString(142 142, 158 158)'));
INSERT INTO t1  VALUES (141, ST_GeomFromText('LineString(141 141, 159 159)'));
INSERT INTO t1  VALUES (140, ST_GeomFromText('LineString(140 140, 160 160)'));
INSERT INTO t1  VALUES (139, ST_GeomFromText('LineString(139 139, 161 161)'));
INSERT INTO t1  VALUES (138, ST_GeomFromText('LineString(138 138, 162 162)'));
INSERT INTO t1  VALUES (137, ST_GeomFromText('LineString(137 137, 163 163)'));
INSERT INTO t1  VALUES (136, ST_GeomFromText('LineString(136 136, 164 164)'));
INSERT INTO t1  VALUES (135, ST_GeomFromText('LineString(135 135, 165 165)'));
INSERT INTO t1  VALUES (134, ST_GeomFromText('LineString(134 134, 166 166)'));
INSERT INTO t1  VALUES (133, ST_GeomFromText('LineString(133 133, 167 167)'));
INSERT INTO t1  VALUES (132, ST_GeomFromText('LineString(132 132, 168 168)'));
INSERT INTO t1  VALUES (131, ST_GeomFromText('LineString(131 131, 169 169)'));
INSERT INTO t1  VALUES (130, ST_GeomFromText('LineString(130 130, 170 170)'));
INSERT INTO t1  VALUES (129, ST_GeomFromText('LineString(129 129, 171 171)'));
INSERT INTO t1  VALUES (128, ST_GeomFromText('LineString(128 128, 172 172)'));
INSERT INTO t1  VALUES (127, ST_GeomFromText('LineString(127 127, 173 173)'));
INSERT INTO t1  VALUES (126, ST_GeomFromText('LineString(126 126, 174 174)'));
INSERT INTO t1  VALUES (125, ST_GeomFromText('LineString(125 125, 175 175)'));
INSERT INTO t1  VALUES (124, ST_GeomFromText('LineString(124 124, 176 176)'));
INSERT INTO t1  VALUES (123, ST_GeomFromText('LineString(123 123, 177 177)'));
INSERT INTO t1  VALUES (122, ST_GeomFromText('LineString(122 122, 178 178)'));
INSERT INTO t1  VALUES (121, ST_GeomFromText('LineString(121 121, 179 179)'));
INSERT INTO t1  VALUES (120, ST_GeomFromText('LineString(120 120, 180 180)'));
INSERT INTO t1  VALUES (119, ST_GeomFromText('LineString(119 119, 181 181)'));
INSERT INTO t1  VALUES (118, ST_GeomFromText('LineString(118 118, 182 182)'));
INSERT INTO t1  VALUES (117, ST_GeomFromText('LineString(117 117, 183 183)'));
INSERT INTO t1  VALUES (116, ST_GeomFromText('LineString(116 116, 184 184)'));
INSERT INTO t1  VALUES (115, ST_GeomFromText('LineString(115 115, 185 185)'));
INSERT INTO t1  VALUES (114, ST_GeomFromText('LineString(114 114, 186 186)'));
INSERT INTO t1  VALUES (113, ST_GeomFromText('LineString(113 113, 187 187)'));
INSERT INTO t1  VALUES (112, ST_GeomFromText('LineString(112 112, 188 188)'));
INSERT INTO t1  VALUES (111, ST_GeomFromText('LineString(111 111, 189 189)'));
INSERT INTO t1  VALUES (110, ST_GeomFromText('LineString(110 110, 190 190)'));
INSERT INTO t1  VALUES (109, ST_GeomFromText('LineString(109 109, 191 191)'));
INSERT INTO t1  VALUES (108, ST_GeomFromText('LineString(108 108, 192 192)'));
INSERT INTO t1  VALUES (107, ST_GeomFromText('LineString(107 107, 193 193)'));
INSERT INTO t1  VALUES (106, ST_GeomFromText('LineString(106 106, 194 194)'));
INSERT INTO t1  VALUES (105, ST_GeomFromText('LineString(105 105, 195 195)'));
INSERT INTO t1  VALUES (104, ST_GeomFromText('LineString(104 104, 196 196)'));
INSERT INTO t1  VALUES (103, ST_GeomFromText('LineString(103 103, 197 197)'));
INSERT INTO t1  VALUES (102, ST_GeomFromText('LineString(102 102, 198 198)'));
INSERT INTO t1  VALUES (101, ST_GeomFromText('LineString(101 101, 199 199)'));
INSERT INTO t1  VALUES (100, ST_GeomFromText('LineString(100 100, 200 200)'));
INSERT INTO t1  VALUES (99, ST_GeomFromText('LineString(99 99, 201 201)'));
INSERT INTO t1  VALUES (98, ST_GeomFromText('LineString(98 98, 202 202)'));
INSERT INTO t1  VALUES (97, ST_GeomFromText('LineString(97 97, 203 203)'));
INSERT INTO t1  VALUES (96, ST_GeomFromText('LineString(96 96, 204 204)'));
INSERT INTO t1  VALUES (95, ST_GeomFromText('LineString(95 95, 205 205)'));
INSERT INTO t1  VALUES (94, ST_GeomFromText('LineString(94 94, 206 206)'));
INSERT INTO t1  VALUES (93, ST_GeomFromText('LineString(93 93, 207 207)'));
INSERT INTO t1  VALUES (92, ST_GeomFromText('LineString(92 92, 208 208)'));
INSERT INTO t1  VALUES (91, ST_GeomFromText('LineString(91 91, 209 209)'));
INSERT INTO t1  VALUES (90, ST_GeomFromText('LineString(90 90, 210 210)'));
INSERT INTO t1  VALUES (89, ST_GeomFromText('LineString(89 89, 211 211)'));
INSERT INTO t1  VALUES (88, ST_GeomFromText('LineString(88 88, 212 212)'));
INSERT INTO t1  VALUES (87, ST_GeomFromText('LineString(87 87, 213 213)'));
INSERT INTO t1  VALUES (86, ST_GeomFromText('LineString(86 86, 214 214)'));
INSERT INTO t1  VALUES (85, ST_GeomFromText('LineString(85 85, 215 215)'));
INSERT INTO t1  VALUES (84, ST_GeomFromText('LineString(84 84, 216 216)'));
INSERT INTO t1  VALUES (83, ST_GeomFromText('LineString(83 83, 217 217)'));
INSERT INTO t1  VALUES (82, ST_GeomFromText('LineString(82 82, 218 218)'));
INSERT INTO t1  VALUES (81, ST_GeomFromText('LineString(81 81, 219 219)'));
INSERT INTO t1  VALUES (80, ST_GeomFromText('LineString(80 80, 220 220)'));
INSERT INTO t1  VALUES (79, ST_GeomFromText('LineString(79 79, 221 221)'));
INSERT INTO t1  VALUES (78, ST_GeomFromText('LineString(78 78, 222 222)'));
INSERT INTO t1  VALUES (77, ST_GeomFromText('LineString(77 77, 223 223)'));
INSERT INTO t1  VALUES (76, ST_GeomFromText('LineString(76 76, 224 224)'));
INSERT INTO t1  VALUES (75, ST_GeomFromText('LineString(75 75, 225 225)'));
INSERT INTO t1  VALUES (74, ST_GeomFromText('LineString(74 74, 226 226)'));
INSERT INTO t1  VALUES (73, ST_GeomFromText('LineString(73 73, 227 227)'));
INSERT INTO t1  VALUES (72, ST_GeomFromText('LineString(72 72, 228 228)'));
INSERT INTO t1  VALUES (71, ST_GeomFromText('LineString(71 71, 229 229)'));
INSERT INTO t1  VALUES (70, ST_GeomFromText('LineString(70 70, 230 230)'));
INSERT INTO t1  VALUES (69, ST_GeomFromText('LineString(69 69, 231 231)'));
INSERT INTO t1  VALUES (68, ST_GeomFromText('LineString(68 68, 232 232)'));
INSERT INTO t1  VALUES (67, ST_GeomFromText('LineString(67 67, 233 233)'));
INSERT INTO t1  VALUES (66, ST_GeomFromText('LineString(66 66, 234 234)'));
INSERT INTO t1  VALUES (65, ST_GeomFromText('LineString(65 65, 235 235)'));
INSERT INTO t1  VALUES (64, ST_GeomFromText('LineString(64 64, 236 236)'));
INSERT INTO t1  VALUES (63, ST_GeomFromText('LineString(63 63, 237 237)'));
INSERT INTO t1  VALUES (62, ST_GeomFromText('LineString(62 62, 238 238)'));
INSERT INTO t1  VALUES (61, ST_GeomFromText('LineString(61 61, 239 239)'));
INSERT INTO t1  VALUES (60, ST_GeomFromText('LineString(60 60, 240 240)'));
INSERT INTO t1  VALUES (59, ST_GeomFromText('LineString(59 59, 241 241)'));
INSERT INTO t1  VALUES (58, ST_GeomFromText('LineString(58 58, 242 242)'));
INSERT INTO t1  VALUES (57, ST_GeomFromText('LineString(57 57, 243 243)'));
INSERT INTO t1  VALUES (56, ST_GeomFromText('LineString(56 56, 244 244)'));
INSERT INTO t1  VALUES (55, ST_GeomFromText('LineString(55 55, 245 245)'));
INSERT INTO t1  VALUES (54, ST_GeomFromText('LineString(54 54, 246 246)'));
INSERT INTO t1  VALUES (53, ST_GeomFromText('LineString(53 53, 247 247)'));
INSERT INTO t1  VALUES (52, ST_GeomFromText('LineString(52 52, 248 248)'));
INSERT INTO t1  VALUES (51, ST_GeomFromText('LineString(51 51, 249 249)'));
INSERT INTO t1  VALUES (50, ST_GeomFromText('LineString(50 50, 250 250)'));
INSERT INTO t1  VALUES (49, ST_GeomFromText('LineString(49 49, 251 251)'));
INSERT INTO t1  VALUES (48, ST_GeomFromText('LineString(48 48, 252 252)'));
INSERT INTO t1  VALUES (47, ST_GeomFromText('LineString(47 47, 253 253)'));
INSERT INTO t1  VALUES (46, ST_GeomFromText('LineString(46 46, 254 254)'));
INSERT INTO t1  VALUES (45, ST_GeomFromText('LineString(45 45, 255 255)'));
INSERT INTO t1  VALUES (44, ST_GeomFromText('LineString(44 44, 256 256)'));
INSERT INTO t1  VALUES (43, ST_GeomFromText('LineString(43 43, 257 257)'));
INSERT INTO t1  VALUES (42, ST_GeomFromText('LineString(42 42, 258 258)'));
INSERT INTO t1  VALUES (41, ST_GeomFromText('LineString(41 41, 259 259)'));
INSERT INTO t1  VALUES (40, ST_GeomFromText('LineString(40 40, 260 260)'));
INSERT INTO t1  VALUES (39, ST_GeomFromText('LineString(39 39, 261 261)'));
INSERT INTO t1  VALUES (38, ST_GeomFromText('LineString(38 38, 262 262)'));
INSERT INTO t1  VALUES (37, ST_GeomFromText('LineString(37 37, 263 263)'));
INSERT INTO t1  VALUES (36, ST_GeomFromText('LineString(36 36, 264 264)'));
INSERT INTO t1  VALUES (35, ST_GeomFromText('LineString(35 35, 265 265)'));
INSERT INTO t1  VALUES (34, ST_GeomFromText('LineString(34 34, 266 266)'));
INSERT INTO t1  VALUES (33, ST_GeomFromText('LineString(33 33, 267 267)'));
INSERT INTO t1  VALUES (32, ST_GeomFromText('LineString(32 32, 268 268)'));
INSERT INTO t1  VALUES (31, ST_GeomFromText('LineString(31 31, 269 269)'));
INSERT INTO t1  VALUES (30, ST_GeomFromText('LineString(30 30, 270 270)'));
INSERT INTO t1  VALUES (29, ST_GeomFromText('LineString(29 29, 271 271)'));
INSERT INTO t1  VALUES (28, ST_GeomFromText('LineString(28 28, 272 272)'));
INSERT INTO t1  VALUES (27, ST_GeomFromText('LineString(27 27, 273 273)'));
INSERT INTO t1  VALUES (26, ST_GeomFromText('LineString(26 26, 274 274)'));
INSERT INTO t1  VALUES (25, ST_GeomFromText('LineString(25 25, 275 275)'));
INSERT INTO t1  VALUES (24, ST_GeomFromText('LineString(24 24, 276 276)'));
INSERT INTO t1  VALUES (23, ST_GeomFromText('LineString(23 23, 277 277)'));
INSERT INTO t1  VALUES (22, ST_GeomFromText('LineString(22 22, 278 278)'));
INSERT INTO t1  VALUES (21, ST_GeomFromText('LineString(21 21, 279 279)'));
INSERT INTO t1  VALUES (20, ST_GeomFromText('LineString(20 20, 280 280)'));
INSERT INTO t1  VALUES (19, ST_GeomFromText('LineString(19 19, 281 281)'));
INSERT INTO t1  VALUES (18, ST_GeomFromText('LineString(18 18, 282 282)'));
INSERT INTO t1  VALUES (17, ST_GeomFromText('LineString(17 17, 283 283)'));
INSERT INTO t1  VALUES (16, ST_GeomFromText('LineString(16 16, 284 284)'));
INSERT INTO t1  VALUES (15, ST_GeomFromText('LineString(15 15, 285 285)'));
INSERT INTO t1  VALUES (14, ST_GeomFromText('LineString(14 14, 286 286)'));
INSERT INTO t1  VALUES (13, ST_GeomFromText('LineString(13 13, 287 287)'));
INSERT INTO t1  VALUES (12, ST_GeomFromText('LineString(12 12, 288 288)'));
INSERT INTO t1  VALUES (11, ST_GeomFromText('LineString(11 11, 289 289)'));
INSERT INTO t1  VALUES (10, ST_GeomFromText('LineString(10 10, 290 290)'));
INSERT INTO t1  VALUES (9, ST_GeomFromText('LineString(9 9, 291 291)'));
INSERT INTO t1  VALUES (8, ST_GeomFromText('LineString(8 8, 292 292)'));
INSERT INTO t1  VALUES (7, ST_GeomFromText('LineString(7 7, 293 293)'));
INSERT INTO t1  VALUES (6, ST_GeomFromText('LineString(6 6, 294 294)'));
INSERT INTO t1  VALUES (5, ST_GeomFromText('LineString(5 5, 295 295)'));
INSERT INTO t1  VALUES (4, ST_GeomFromText('LineString(4 4, 296 296)'));
INSERT INTO t1  VALUES (3, ST_GeomFromText('LineString(3 3, 297 297)'));
INSERT INTO t1  VALUES (2, ST_GeomFromText('LineString(2 2, 298 298)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(1 1, 299 299)'));
select count(*) from t1;
count(*)
150
set @g1 = ST_GeomFromText('Polygon((0 0,0 1000,1000 1000,1000 0,0 0))');
select count(*) from t1 where MBRwithin(t1.c2, @g1);
count(*)
150
truncate t1;
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(2 2, 150 150)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(3 3, 160 160)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(4 4, 170 170)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(5 5, 180 180)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(6 6, 190 190)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(7 7, 200 200)'));
INSERT INTO t1  VALUES (1, ST_GeomFromText('LineString(8 8, 210 210)'));
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
set @g1 = ST_GeomFromText('Polygon((0 0,0 2, 2 2, 2 0, 0 0))');
select count(*) from t1 where MBRtouches(t1.c2, @g1);
count(*)
2048
set @g1 = ST_GeomFromText('Polygon((0 0,0 200,200 200,200 0,0 0))');
select count(*) from t1 where MBRWithin(t1.c2, @g1);
count(*)
12288
set @g1 = ST_GeomFromText('LineString(2 2, 150 150)');
select count(*) from t1 where MBRequals(t1.c2, @g1);
count(*)
2048
set @g1 = ST_GeomFromText('Polygon((0 0,0 200,200 200,200 0,0 0))');
create table t3 (a int) engine = innodb;
CREATE PROCEDURE curdemo()
BEGIN
DECLARE done INT DEFAULT FALSE;
DECLARE a INT;
DECLARE cur1 CURSOR FOR SELECT c1 from t1 where  MBRWithin(t1.c2, @g1);
DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;
OPEN cur1;
read_loop: LOOP
FETCH cur1 INTO a;
IF done THEN
LEAVE read_loop;
END IF;
INSERT INTO test.t3 VALUES (a);
END LOOP;
CLOSE cur1;
END|
call curdemo();
select count(*) from t3;
count(*)
12288
drop procedure curdemo;
drop table t3;
drop table t1;
