INSTALL COMPONENT 'file://component_test_table_access';
SELECT test_table_access_driver("unknown test");
test_table_access_driver("unknown test")
NULL
SELECT test_table_access_driver("INSERT-CUSTOMER-STRESS-1");
test_table_access_driver("INSERT-CUSTOMER-STRESS-1")
create() failed
SELECT test_table_access_driver("INSERT-CUSTOMER-STRESS-2");
test_table_access_driver("INSERT-CUSTOMER-STRESS-2")
get() failed
SELECT test_table_access_driver("INSERT-CUSTOMER");
test_table_access_driver("INSERT-CUSTOMER")
get() failed
CREATE SCHEMA shop;
SELECT test_table_access_driver("INSERT-CUSTOMER");
test_table_access_driver("INSERT-CUSTOMER")
get() failed
CREATE TABLE shop.customer (x integer);
SELECT test_table_access_driver("INSERT-CUSTOMER-STRESS-3");
test_table_access_driver("INSERT-CUSTOMER-STRESS-3")
get() failed
SELECT test_table_access_driver("INSERT-CUSTOMER");
test_table_access_driver("INSERT-CUSTOMER")
check() failed
DROP TABLE shop.customer;
CREATE TABLE shop.customer(x integer, y integer, z integer);
SELECT test_table_access_driver("INSERT-CUSTOMER");
test_table_access_driver("INSERT-CUSTOMER")
check() failed
DROP TABLE shop.customer;
CREATE TABLE shop.customer(
`ID` integer not null,
`NAME` varchar(50) not null,
`ADDRESS` varchar(255));
SELECT test_table_access_driver("INSERT-CUSTOMER-NO-COMMIT");
test_table_access_driver("INSERT-CUSTOMER-NO-COMMIT")
OK, but forgot to commit
SELECT * from shop.customer;
ID	NAME	ADDRESS
SELECT test_table_access_driver("INSERT-CUSTOMER-ROLLBACK");
test_table_access_driver("INSERT-CUSTOMER-ROLLBACK")
OK
SELECT * from shop.customer;
ID	NAME	ADDRESS
SELECT test_table_access_driver("INSERT-CUSTOMER");
test_table_access_driver("INSERT-CUSTOMER")
OK
SELECT * from shop.customer;
ID	NAME	ADDRESS
1	John Doe	NULL
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
get(order) failed
CREATE TABLE `shop`.`order` (x integer);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
check(order) failed
DROP TABLE `shop`.`order`;
CREATE TABLE `shop`.`order` (
CUSTOMER_ID INTEGER not null,
ORDER_ID INTEGER not null,
ORDER_COMMENT VARCHAR(50),
DATE_CREATED DATE);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
get(order_line) failed
CREATE TABLE `shop`.`order_line` (y integer);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
check(order_line) failed
DROP TABLE `shop`.`order_line`;
CREATE TABLE `shop`.`order_line` (
ORDER_ID INTEGER not null,
LINE_NUM INTEGER not null,
ITEM_ID INTEGER not null,
UNIT_PRICE DECIMAL(5,2) not null,
QTY INTEGER not null);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
init(order::pk) failed
ALTER TABLE `shop`.`order` ADD INDEX index_date_created(DATE_CREATED);
SELECT TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, IS_VISIBLE
FROM INFORMATION_SCHEMA.STATISTICS
WHERE TABLE_SCHEMA = 'shop'
  ORDER BY TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
TABLE_SCHEMA	TABLE_NAME	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	IS_VISIBLE
shop	order	index_date_created	1	DATE_CREATED	YES
SHOW CREATE TABLE `shop`.`order`;
Table	Create Table
order	CREATE TABLE `order` (
  `CUSTOMER_ID` int NOT NULL,
  `ORDER_ID` int NOT NULL,
  `ORDER_COMMENT` varchar(50) DEFAULT NULL,
  `DATE_CREATED` date DEFAULT NULL,
  KEY `index_date_created` (`DATE_CREATED`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
init(order::pk) failed
ALTER TABLE `shop`.`order` ADD KEY(ORDER_ID);
SELECT TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, IS_VISIBLE
FROM INFORMATION_SCHEMA.STATISTICS
WHERE TABLE_SCHEMA = 'shop'
  ORDER BY TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
TABLE_SCHEMA	TABLE_NAME	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	IS_VISIBLE
shop	order	index_date_created	1	DATE_CREATED	YES
shop	order	ORDER_ID	1	ORDER_ID	YES
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
init(order::pk) failed
ALTER TABLE `shop`.`order` DROP KEY ORDER_ID;
ALTER TABLE `shop`.`order` ADD PRIMARY KEY(ORDER_ID);
SELECT TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, IS_VISIBLE
FROM INFORMATION_SCHEMA.STATISTICS
WHERE TABLE_SCHEMA = 'shop'
  ORDER BY TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
TABLE_SCHEMA	TABLE_NAME	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	IS_VISIBLE
shop	order	index_date_created	1	DATE_CREATED	YES
shop	order	PRIMARY	1	ORDER_ID	YES
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
No such order
INSERT INTO `shop`.`order` VALUES(1, 1000, 'This is order 1000', '2023-04-01');
INSERT INTO `shop`.`order` VALUES(1, 1002, 'This is order 1002', '2048-04-01');
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
No such order
INSERT INTO `shop`.`order` VALUES(1, 1001, 'This is order 1001', '2020-12-31');
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
init(order_line::pk) failed
ALTER TABLE `shop`.`order_line` ADD PRIMARY KEY(ORDER_ID, LINE_NUM);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
found: (This is order 1001), no order line
INSERT INTO `shop`.`order_line` VALUES(1000, 1, 888, 12.23, 10);
INSERT INTO `shop`.`order_line` VALUES(1000, 2, 777, 34.45, 20);
INSERT INTO `shop`.`order_line` VALUES(1000, 3, 666, 8.99, 5);
INSERT INTO `shop`.`order_line` VALUES(1002, 1, 555, 100, 1);
INSERT INTO `shop`.`order_line` VALUES(1002, 2, 444, 0.99, 1000);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
found: (This is order 1001), no order line
INSERT INTO `shop`.`order_line` VALUES(1001, 1, 333, 12, 1);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
found: (This is order 1001), total qty: 1
INSERT INTO `shop`.`order_line` VALUES(1001, 2, 222, 24, 5);
SELECT test_table_access_driver("FETCH-ORDER");
test_table_access_driver("FETCH-ORDER")
found: (This is order 1001), total qty: 6
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
get(warehouse) failed
CREATE TABLE `shop`.`warehouse` (BUILDING_ID integer);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
check(warehouse) failed
DROP TABLE `shop`.`warehouse`;
CREATE TABLE `shop`.`warehouse` (
BUILDING_ID integer not null,
FLOOR_NUMBER integer not null,
ALLEY_NUMBER integer not null,
SHELVE_NUMBER integer not null,
CAPACITY integer not null,
DESCRIPTION VARCHAR(100));
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
init(shelves) failed
ALTER TABLE `shop`.`warehouse` ADD INDEX some_index(DESCRIPTION);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
init(shelves) failed
ALTER TABLE `shop`.`warehouse` ADD INDEX `SHELVES`(SHELVE_NUMBER);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
init(shelves) failed
ALTER TABLE `shop`.`warehouse` DROP INDEX `SHELVES`;
ALTER TABLE `shop`.`warehouse` ADD INDEX `SHELVES`(
SHELVE_NUMBER, ALLEY_NUMBER, FLOOR_NUMBER, BUILDING_ID);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
init(shelves) failed
ALTER TABLE `shop`.`warehouse` DROP INDEX `SHELVES`;
ALTER TABLE `shop`.`warehouse` ADD INDEX `SHELVES`(
BUILDING_ID, ALLEY_NUMBER, FLOOR_NUMBER, SHELVE_NUMBER);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
init(shelves) failed
ALTER TABLE `shop`.`warehouse` DROP INDEX `SHELVES`;
ALTER TABLE `shop`.`warehouse` ADD INDEX `SHELVES`(
BUILDING_ID, FLOOR_NUMBER, ALLEY_NUMBER, SHELVE_NUMBER, DESCRIPTION);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
init(shelves) failed
ALTER TABLE `shop`.`warehouse` DROP INDEX `SHELVES`;
ALTER TABLE `shop`.`warehouse` ADD INDEX `SHELVES`(
BUILDING_ID, FLOOR_NUMBER, ALLEY_NUMBER, SHELVE_NUMBER);
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
No shelve with min capacity (100) in anywhere
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
No shelve with min capacity (100) in B:1005
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
No shelve with min capacity (100) in B:1005 F:5
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
No shelve with min capacity (100) in B:1005 F:5 A:5
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
No shelve with min capacity (100) in B:1005 F:5 A:5 S:5
INSERT INTO `shop`.`warehouse` VALUES(1007, 4, 3, 12, 50, "no comments");
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
No shelve with min capacity (100) in anywhere
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
No shelve with min capacity (100) in B:1005
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
No shelve with min capacity (100) in B:1005 F:5
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
No shelve with min capacity (100) in B:1005 F:5 A:5
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
No shelve with min capacity (100) in B:1005 F:5 A:5 S:5
INSERT INTO `shop`.`warehouse` VALUES(1007, 4, 3, 11, 150, "no comments");
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
Found capacity (150) for min (100) at B:1007 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
No shelve with min capacity (100) in B:1005
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
No shelve with min capacity (100) in B:1005 F:5
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
No shelve with min capacity (100) in B:1005 F:5 A:5
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
No shelve with min capacity (100) in B:1005 F:5 A:5 S:5
INSERT INTO `shop`.`warehouse` VALUES(1003, 4, 3, 11, 250, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 6, 3, 11, 6311, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 4, 3, 11, 4311, "no comments");
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
Found capacity (250) for min (100) at B:1003 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
Found capacity (4311) for min (100) at B:1005 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
No shelve with min capacity (100) in B:1005 F:5
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
No shelve with min capacity (100) in B:1005 F:5 A:5
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
No shelve with min capacity (100) in B:1005 F:5 A:5 S:5
INSERT INTO `shop`.`warehouse` VALUES(1003, 4, 3, 11, 250, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 7, 18, 5718, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 2, 19, 5219, "no comments");
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
Found capacity (250) for min (100) at B:1003 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
Found capacity (4311) for min (100) at B:1005 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
Found capacity (5219) for min (100) at B:1005 F:5 A:2 S:19
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
No shelve with min capacity (100) in B:1005 F:5 A:5
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
No shelve with min capacity (100) in B:1005 F:5 A:5 S:5
INSERT INTO `shop`.`warehouse` VALUES(1002, 2, 2, 2, 20, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 5, 33, 5533, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 5, 22, 5522, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 5, 11, 11, "no comments");
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
Found capacity (250) for min (100) at B:1003 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
Found capacity (4311) for min (100) at B:1005 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
Found capacity (5219) for min (100) at B:1005 F:5 A:2 S:19
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
Found capacity (5522) for min (100) at B:1005 F:5 A:5 S:22
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
No shelve with min capacity (100) in B:1005 F:5 A:5 S:5
INSERT INTO `shop`.`warehouse` VALUES(1001, 5, 5, 5, 12, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 5, 2, 552, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 5, 7, 557, "no comments");
INSERT INTO `shop`.`warehouse` VALUES(1005, 5, 5, 5, 5555, "no comments");
SELECT test_table_access_driver("INDEX-SCAN");
test_table_access_driver("INDEX-SCAN")
Found capacity (250) for min (100) at B:1003 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-B");
test_table_access_driver("INDEX-FETCH-B")
Found capacity (4311) for min (100) at B:1005 F:4 A:3 S:11
SELECT test_table_access_driver("INDEX-FETCH-BF");
test_table_access_driver("INDEX-FETCH-BF")
Found capacity (5219) for min (100) at B:1005 F:5 A:2 S:19
SELECT test_table_access_driver("INDEX-FETCH-BFA");
test_table_access_driver("INDEX-FETCH-BFA")
Found capacity (552) for min (100) at B:1005 F:5 A:5 S:2
SELECT test_table_access_driver("INDEX-FETCH-BFAS");
test_table_access_driver("INDEX-FETCH-BFAS")
Found capacity (5555) for min (100) at B:1005 F:5 A:5 S:5
DROP TABLE shop.warehouse;
DROP TABLE shop.customer;
DROP SCHEMA shop;
UNINSTALL COMPONENT 'file://component_test_table_access';
