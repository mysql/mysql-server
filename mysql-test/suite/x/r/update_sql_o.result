=============================================
SQL UPDATE SCENARIOS
=============================================

================================================================================
PREAMBLE
================================================================================
create user user2@localhost identified by 'user2';
grant all on *.* to user2@localhost;
================================================================================
TEST START
================================================================================
RUN DROP SCHEMA if EXISTS mysqlxplugin

0 rows affected
RUN CREATE SCHEMA mysqlxplugin DEFAULT CHARSET='utf8'

1 rows affected
RUN USE mysqlxplugin

0 rows affected
RUN CREATE TABLE categories (
 CategoryID int NOT NULL AUTO_INCREMENT,
 CategoryName varchar(100),
 CategoryDescription varchar(200),
 CategoryIMEI tinyint,	  
 CategoryDecimal decimal(5,2),
 PRIMARY key (CategoryID)
 )

0 rows affected
RUN INSERT INTO categories(CategoryID, CategoryName, CategoryDescription, CategoryIMEI, CategoryDecimal)
 VALUES
 (1,'Sports','Sports related category',1,235.15),
 (2,'Entertaiment','Entertaiment related category',2,235.15),
 (3, 'Home','Home related category',3,235.15),
 (4, 'Kitchen','Kitchen related category',4,235.15)

4 rows affected
last insert id: 4
Records: 4  Duplicates: 0  Warnings: 0
RUN UPDATE categories SET CategoryName = 'Sports Update' where CategoryID =1

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET CategoryIMEI = 110 where CategoryID = 1

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET CategoryDecimal = 613.47 where CategoryID =1

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET 
 CategoryName='"Quotes"', 
 CategoryDescription='Quote Categories', 
 CategoryIMEI = 3, 
 CategoryDecimal = 235.15 
 WHERE CategoryID = 3

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET 
 CategoryName=NULL, 
 CategoryDescription=NULL, 
 CategoryIMEI = NULL, 
 CategoryDecimal = NULL
 WHERE CategoryID = 4

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET CategoryDecimal = 999.99
 WHERE CategoryID = 3

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET CategoryDecimal = -999.99
 WHERE CategoryID = 3

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET CategoryIMEI = 127
 WHERE CategoryID = 4

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN UPDATE categories SET CategoryIMEI = -128
 WHERE CategoryID = 4

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Mysqlx.Ok {
  msg: "bye!"
}
ok
.
#---------- START NEGATIVE TEST ----------#
.
RUN USE mysqlxplugin

0 rows affected
RUN UPDATE categories SET CategoryNoExist = 2 where CategoryID =1
While executing UPDATE categories SET CategoryNoExist = 2 where CategoryID =1:
Got expected error: Unknown column 'CategoryNoExist' in 'field list' (code 1054)
RUN UPDATE categories set CategoryIMEI = 'TinyInt' WHERE CategoryID = 2
While executing UPDATE categories set CategoryIMEI = 'TinyInt' WHERE CategoryID = 2:
Got expected error: Incorrect integer value: 'TinyInt' for column 'CategoryIMEI' at row 1 (code 1366)
RUN UPDATE categories set CategoryDecimal = 'Decimal' WHERE CategoryID = 2
While executing UPDATE categories set CategoryDecimal = 'Decimal' WHERE CategoryID = 2:
Got expected error: Incorrect decimal value: 'Decimal' for column 'CategoryDecimal' at row 1 (code 1366)
RUN UPDATE categories SET CategoryDecimal = 1000.00 WHERE CategoryID = 3
While executing UPDATE categories SET CategoryDecimal = 1000.00 WHERE CategoryID = 3:
Got expected error: Out of range value for column 'CategoryDecimal' at row 1 (code 1264)
RUN UPDATE categories SET CategoryDecimal = -1000.00 WHERE CategoryID = 3
While executing UPDATE categories SET CategoryDecimal = -1000.00 WHERE CategoryID = 3:
Got expected error: Out of range value for column 'CategoryDecimal' at row 1 (code 1264)
RUN UPDATE categories SET CategoryIMEI = 128 WHERE CategoryID = 4
While executing UPDATE categories SET CategoryIMEI = 128 WHERE CategoryID = 4:
Got expected error: Out of range value for column 'CategoryIMEI' at row 1 (code 1264)
RUN UPDATE categories SET CategoryIMEI = -129 WHERE CategoryID = 4
While executing UPDATE categories SET CategoryIMEI = -129 WHERE CategoryID = 4:
Got expected error: Out of range value for column 'CategoryIMEI' at row 1 (code 1264)
RUN UPDATE categories_no_exist set CategoryIMEI = 'TinyInt' WHERE CategoryID = 2
While executing UPDATE categories_no_exist set CategoryIMEI = 'TinyInt' WHERE CategoryID = 2:
Got expected error: Table 'mysqlxplugin.categories_no_exist' doesn't exist (code 1146)
================================================================================
CLEAN UP
================================================================================
RUN DROP SCHEMA if EXISTS mysqlxplugin

1 rows affected
RUN DROP USER user2@localhost

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
