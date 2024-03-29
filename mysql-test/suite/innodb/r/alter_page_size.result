#
# Bug #19077964 ASSERT PAGE_SIZE.EQUALS_TO SPACE_PAGE_SIZE
# BTR_COPY_BLOB_PREFIX
#
create table t1 (f1 int primary key, f3 linestring not null,
spatial key(f3)) engine=innodb row_format=compressed key_block_size=1;
Warnings:
Warning	3674	The spatial index on column 'f3' will not be used by the query optimizer since the column does not have an SRID attribute. Consider adding an SRID attribute to the column.
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f3` linestring NOT NULL,
  PRIMARY KEY (`f1`),
  SPATIAL KEY `f3` (`f3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=1
insert into t1 values (1, ST_linefromtext(concat('linestring', '( 0 0, 1 1, 2 2, 3 3, 4 4, 5 5, 6 6, 7 7, 8 8, 9 9, 10 10, 11 11, 12 12, 13 13, 14 14, 15 15, 16 16, 17 17, 18 18, 19 19, 20 20, 21 21, 22 22, 23 23, 24 24, 25 25, 26 26, 27 27, 28 28, 29 29, 30 30, 31 31, 32 32, 33 33, 34 34, 35 35, 36 36, 37 37, 38 38, 39 39, 40 40, 41 41, 42 42, 43 43, 44 44, 45 45, 46 46, 47 47, 48 48, 49 49, 50 50, 51 51, 52 52, 53 53, 54 54, 55 55, 56 56, 57 57, 58 58, 59 59, 60 60, 61 61, 62 62, 63 63, 64 64, 65 65, 66 66, 67 67, 68 68, 69 69, 70 70, 71 71, 72 72, 73 73, 74 74, 75 75, 76 76, 77 77, 78 78, 79 79, 9999 9999)')));;
alter table t1 row_format=dynamic, key_block_size=0, algorithm=inplace;
drop table t1;
