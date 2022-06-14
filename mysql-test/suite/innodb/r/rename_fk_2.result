#
# Bug #27453180 FOREIGN KEYS CONSTRAINTS IGNORED AFTER RENAME TABLE
#
SET foreign_key_checks=0;
CREATE TABLE `country` (
`country_id` SMALLINT(5) UNSIGNED NOT NULL AUTO_INCREMENT,
`country` VARCHAR(50) NOT NULL,
`last_update` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
PRIMARY KEY (`country_id`)
) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TABLE `city` (
`city_id` SMALLINT(5) UNSIGNED NOT NULL AUTO_INCREMENT,
`city` VARCHAR(50) NOT NULL,
`country_id` SMALLINT(5) UNSIGNED NOT NULL,
`last_update` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
PRIMARY KEY (`city_id`),
KEY `idx_fk_country_id` (`country_id`),
CONSTRAINT `fk_city_country` FOREIGN KEY (`country_id`) REFERENCES `country` (`country_id`) ON UPDATE CASCADE
) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TABLE `address` (
`address_id` SMALLINT(5) UNSIGNED NOT NULL AUTO_INCREMENT,
`address` VARCHAR(50) NOT NULL,
`city_id` SMALLINT(5) UNSIGNED NOT NULL,
`postal_code` VARCHAR(10) DEFAULT NULL,
`last_update` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
PRIMARY KEY (`address_id`),
KEY `idx_fk_city_id` (`city_id`),
CONSTRAINT `fk_address_city` FOREIGN KEY (`city_id`) REFERENCES `city` (`city_id`) ON UPDATE CASCADE
) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO country (`country_id`, `country`) VALUES
(1, 'Canada'),
(2, 'USA'),
(3, 'Mexico'),
(4, 'France'),
(5, 'Spain');
INSERT INTO city (`city_id`, `city`, `country_id`) VALUES
(1, 'Montral', 1),
(2, 'New York', 2),
(3, 'Durango',  3),
(4, 'Paris',    4),
(5, 'Madrid',   5);
INSERT INTO address (`address_id`, `address`, `city_id`, `postal_code`) VALUES
(1, 'addy 1', 1, '10000'),
(2, 'addy 2', 2, '20000'),
(3, 'addy 3', 3, '30000'),
(4, 'addy 4', 4, '40000'),
(5, 'addy 5', 5, '50000');
SET foreign_key_checks=1;
DELETE FROM country WHERE country_id =1;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails (`test`.`city`, CONSTRAINT `fk_city_country` FOREIGN KEY (`country_id`) REFERENCES `country` (`country_id`) ON UPDATE CASCADE)
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN;
ID	FOR_NAME	REF_NAME	N_COLS	TYPE
test/fk_address_city	test/address	test/city	1	20
test/fk_city_country	test/city	test/country	1	20
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS;
ID	FOR_COL_NAME	REF_COL_NAME	POS
test/fk_address_city	city_id	city_id	1
test/fk_city_country	country_id	country_id	1
CREATE TABLE tmp_country LIKE country;
INSERT INTO tmp_country SELECT * FROM country;
SET foreign_key_checks=0;
DROP TABLE country;
RENAME TABLE tmp_country TO country;
SET foreign_key_checks=1;
DELETE FROM country WHERE country_id =1;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails (`test`.`city`, CONSTRAINT `fk_city_country` FOREIGN KEY (`country_id`) REFERENCES `country` (`country_id`) ON UPDATE CASCADE)
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN;
ID	FOR_NAME	REF_NAME	N_COLS	TYPE
test/fk_address_city	test/address	test/city	1	20
test/fk_city_country	test/city	test/country	1	20
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS;
ID	FOR_COL_NAME	REF_COL_NAME	POS
test/fk_address_city	city_id	city_id	1
test/fk_city_country	country_id	country_id	1
DROP TABLE address;
DROP TABLE city;
DROP TABLE country;
#
# Bug #29686796 RENAMING A MEM TABLE WITH FK WHILE FOREIGN_KEY_CHECKS
# IS SET 0 RESULTS IN CRASH
#
CREATE TABLE t1(f1 INT,f2 INT)ENGINE=MEMORY;
set foreign_key_checks=0;
CREATE TABLE t2
(id INT KEY,
t1_id INT,
INDEX par_ind (t1_id,id),
FOREIGN KEY(t1_id) REFERENCES t1(id) ON DELETE CASCADE) ENGINE=INNODB;
RENAME TABLE t1 to t5,t2 to t1;
DROP TABLE t1, t5;
SET foreign_key_checks=1;
