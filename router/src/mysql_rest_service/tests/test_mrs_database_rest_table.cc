/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <map>
#include <vector>

#include "test_mrs_database_rest_table.h"

constexpr const char *k_test_ddl[] = {
    "CREATE SCHEMA mrstestdb", "USE mrstestdb",

    R"*(CREATE TABLE `typetest` (
  id INT PRIMARY KEY,
  geom GEOMETRY DEFAULT NULL,
  bool BIT(1) DEFAULT 0,
  bin BLOB DEFAULT NULL,
  js JSON
))*",

    R"*(INSERT INTO `typetest` VALUES 
    (1, 0x00000000010100000006240626DCD857403C45B357C4753540, 1, 0x68656C6C6F, '{"a": 1}'))*",

    R"*(CREATE TABLE `country` (
  `country_id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `country` varchar(50) NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`country_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `city` (
  `city_id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `city` varchar(50) NOT NULL,
  `country_id` smallint unsigned NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`country_id`, `city_id`),
  KEY `idx_fk_city_id` (`city_id`),
  CONSTRAINT `fk_city_country` FOREIGN KEY (`country_id`) REFERENCES `country` (`country_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `city2` (
  `city_id` smallint unsigned NOT NULL,
  `city` varchar(50) NOT NULL,
  `country_id` smallint unsigned NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`city_id`),
  KEY `idx_fk_city2_id` (`city_id`),
  CONSTRAINT `fk_city2_country` FOREIGN KEY (`country_id`) REFERENCES `country` (`country_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `store` (
  `store_id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `city_country_id` smallint unsigned NOT NULL,
  `city_id` smallint unsigned NOT NULL,  
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`store_id`),
  CONSTRAINT `fk_store_city_country` FOREIGN KEY (`city_country_id`,`city_id`) REFERENCES `city` (`country_id`,`city_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `language` (
  `language_id` tinyint unsigned NOT NULL AUTO_INCREMENT,
  `name` char(20) NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`language_id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `actor` (
  `actor_id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `first_name` varchar(45) NOT NULL,
  `last_name` varchar(45) NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`actor_id`),
  KEY `idx_actor_last_name` (`last_name`)
) ENGINE=InnoDB AUTO_INCREMENT=201 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `film` (
  `film_id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `title` varchar(128) NOT NULL,
  `description` text,
  `release_year` year DEFAULT NULL,
  `language_id` tinyint unsigned NOT NULL DEFAULT 1,
  `original_language_id` tinyint unsigned DEFAULT NULL,
  `rental_duration` tinyint unsigned NOT NULL DEFAULT '3',
  `rental_rate` decimal(4,2) NOT NULL DEFAULT '4.99',
  `length` smallint unsigned DEFAULT NULL,
  `replacement_cost` decimal(5,2) NOT NULL DEFAULT '19.99',
  `rating` enum('G','PG','PG-13','R','NC-17') DEFAULT 'G',
  `special_features` set('Trailers','Commentaries','Deleted Scenes','Behind the Scenes') DEFAULT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`film_id`),
  KEY `idx_title` (`title`),
  KEY `idx_fk_language_id` (`language_id`),
  KEY `idx_fk_original_language_id` (`original_language_id`),
  CONSTRAINT `fk_film_language` FOREIGN KEY (`language_id`) REFERENCES `language` (`language_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_film_language_original` FOREIGN KEY (`original_language_id`) REFERENCES `language` (`language_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=1001 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `film_actor` (
  `actor_id` smallint unsigned NOT NULL,
  `film_id` smallint unsigned NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`actor_id`,`film_id`),
  KEY `idx_fk_film_id` (`film_id`),
  CONSTRAINT `fk_film_actor_actor` FOREIGN KEY (`actor_id`) REFERENCES `actor` (`actor_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_film_actor_film` FOREIGN KEY (`film_id`) REFERENCES `film` (`film_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `film_actor2` (
  `actor_id` smallint unsigned NOT NULL,
  `film_id` smallint unsigned NOT NULL,
  `character` text,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`actor_id`,`film_id`),
  KEY `idx_fk_film_id` (`film_id`),
  CONSTRAINT `fk_film_actor2_actor` FOREIGN KEY (`actor_id`) REFERENCES `actor` (`actor_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_film_actor2_film` FOREIGN KEY (`film_id`) REFERENCES `film` (`film_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `category` (
  `category_id` tinyint unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(25) NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`category_id`)
) ENGINE=InnoDB AUTO_INCREMENT=17 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(CREATE TABLE `film_category` (
  `film_id` smallint unsigned NOT NULL,
  `category_id` tinyint unsigned NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`film_id`,`category_id`),
  KEY `fk_film_category_category` (`category_id`),
  CONSTRAINT `fk_film_category_category` FOREIGN KEY (`category_id`) REFERENCES `category` (`category_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_film_category_film` FOREIGN KEY (`film_id`) REFERENCES `film` (`film_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci)*",

    R"*(INSERT INTO country VALUES (1,'Afghanistan','2006-02-15 04:44:00'),
(2,'Algeria','2006-02-15 04:44:00'),
(3,'American Samoa','2006-02-15 04:44:00'),
(4,'Angola','2006-02-15 04:44:00'),
(5,'Anguilla','2006-02-15 04:44:00'),
(6,'Argentina','2006-02-15 04:44:00'),
(7,'Armenia','2006-02-15 04:44:00'),
(8,'Australia','2006-02-15 04:44:00'),
(9,'Austria','2006-02-15 04:44:00'),
(10,'Azerbaijan','2006-02-15 04:44:00'))*",

    R"*(INSERT INTO `city` VALUES (251,'Kabul',1,'2006-02-15 12:45:25'),
(516,'Tafuna',3,'2006-02-15 12:45:25'),
(67,'Benguela',4,'2006-02-15 12:45:25'),
(360,'Namibe',4,'2006-02-15 12:45:25'),
(493,'South Hill',5,'2006-02-15 12:45:25'),
(20,'Almirante Brown',6,'2006-02-15 12:45:25'),
(43,'Avellaneda',6,'2006-02-15 12:45:25'),
(45,'Baha Blanca',6,'2006-02-15 12:45:25'),
(128,'Crdoba',6,'2006-02-15 12:45:25'),
(161,'Escobar',6,'2006-02-15 12:45:25'),
(165,'Ezeiza',6,'2006-02-15 12:45:25'),
(289,'La Plata',6,'2006-02-15 12:45:25'),
(334,'Merlo',6,'2006-02-15 12:45:25'),
(424,'Quilmes',6,'2006-02-15 12:45:25'),
(454,'San Miguel de Tucumn',6,'2006-02-15 12:45:25'),
(457,'Santa F',6,'2006-02-15 12:45:25'),
(524,'Tandil',6,'2006-02-15 12:45:25'),
(567,'Vicente Lpez',6,'2006-02-15 12:45:25'),
(586,'Yerevan',7,'2006-02-15 12:45:25'),
(576,'Woodridge',8,'2006-02-15 12:45:25'),
(186,'Graz',9,'2006-02-15 12:45:25'),
(307,'Linz',9,'2006-02-15 12:45:25'),
(447,'Salzburg',9,'2006-02-15 12:45:25'),
(48,'Baku',10,'2006-02-15 12:45:25'),
(505,'Sumqayit',10,'2006-02-15 12:45:25'))*",

    R"*(INSERT INTO `store` VALUES (1, 3, 516, '2020-01-01 01:02:03'),
    (2, 9, 186, '2020-01-01 01:02:03'),
    (3, 6, 524, '2020-01-01 01:02:03'),
    (4, 5, 493, '2020-01-01 01:02:03'),
    (5, 3, 516, '2020-01-01 01:02:03'))*",

    R"*(INSERT INTO language VALUES (1,'English','2006-02-15 05:02:19'),
(2,'Italian','2006-02-15 05:02:19'),
(3,'Japanese','2006-02-15 05:02:19'),
(4,'Mandarin','2006-02-15 05:02:19'),
(5,'French','2006-02-15 05:02:19'),
(6,'German','2006-02-15 05:02:19'))*",

    R"*(INSERT INTO category VALUES (1,'Action','2006-02-15 04:46:27'),
(2,'Animation','2006-02-15 04:46:27'),
(3,'Children','2006-02-15 04:46:27'),
(4,'Classics','2006-02-15 04:46:27'),
(5,'Comedy','2006-02-15 04:46:27'),
(6,'Documentary','2006-02-15 04:46:27'),
(7,'Drama','2006-02-15 04:46:27'),
(8,'Family','2006-02-15 04:46:27'),
(9,'Foreign','2006-02-15 04:46:27'),
(10,'Games','2006-02-15 04:46:27'),
(11,'Horror','2006-02-15 04:46:27'),
(12,'Music','2006-02-15 04:46:27'),
(13,'New','2006-02-15 04:46:27'),
(14,'Sci-Fi','2006-02-15 04:46:27'),
(15,'Sports','2006-02-15 04:46:27'),
(16,'Travel','2006-02-15 04:46:27'))*",

    R"*(INSERT INTO actor VALUES (1,'PENELOPE','GUINESS','2006-02-15 04:34:33'),
(2,'NICK','WAHLBERG','2006-02-15 04:34:33'),
(3,'ED','CHASE','2006-02-15 04:34:33'),
(4,'JENNIFER','DAVIS','2006-02-15 04:34:33'),
(5,'JOHNNY','LOLLOBRIGIDA','2006-02-15 04:34:33'),
(6,'BETTE','NICHOLSON','2006-02-15 04:34:33'),
(7,'GRACE','MOSTEL','2006-02-15 04:34:33'),
(8,'MATTHEW','JOHANSSON','2006-02-15 04:34:33'),
(9,'JOE','SWANK','2006-02-15 04:34:33'),
(10,'CHRISTIAN','GABLE','2006-02-15 04:34:33'),
(11,'SOLO','ACTOR','2006-02-15 04:34:33'))*",

    R"*(INSERT INTO film VALUES (1,'ACADEMY DINOSAUR','A Epic Drama of a Feminist And a Mad Scientist who must Battle a Teacher in The Canadian Rockies',2006,1,2,6,'0.99',86,'20.99','PG','Deleted Scenes,Behind the Scenes','2006-02-15 05:03:42'),
(2,'ACE GOLDFINGER','A Astounding Epistle of a Database Administrator And a Explorer who must Find a Car in Ancient China',2006,1,NULL,3,'4.99',48,'12.99','G','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(3,'ADAPTATION HOLES','A Astounding Reflection of a Lumberjack And a Car who must Sink a Lumberjack in A Baloon Factory',2006,1,NULL,7,'2.99',50,'18.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(4,'AFFAIR PREJUDICE','A Fanciful Documentary of a Frisbee And a Lumberjack who must Chase a Monkey in A Shark Tank',2006,1,3,5,'2.99',117,'26.99','G','Commentaries,Behind the Scenes','2006-02-15 05:03:42'),
(5,'AFRICAN EGG','A Fast-Paced Documentary of a Pastry Chef And a Dentist who must Pursue a Forensic Psychologist in The Gulf of Mexico',2006,1,NULL,6,'2.99',130,'22.99','G','Deleted Scenes','2006-02-15 05:03:42'),
(6,'AGENT TRUMAN','A Intrepid Panorama of a Robot And a Boy who must Escape a Sumo Wrestler in Ancient China',2006,1,NULL,3,'2.99',169,'17.99','PG','Deleted Scenes','2006-02-15 05:03:42'),
(7,'AIRPLANE SIERRA','A Touching Saga of a Hunter And a Butler who must Discover a Butler in A Jet Boat',2006,1,NULL,6,'4.99',62,'28.99','PG-13','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(8,'AIRPORT POLLOCK','A Epic Tale of a Moose And a Girl who must Confront a Monkey in Ancient India',2006,1,NULL,6,'4.99',54,'15.99','R','Trailers','2006-02-15 05:03:42'),
(9,'ALABAMA DEVIL','A Thoughtful Panorama of a Database Administrator And a Mad Scientist who must Outgun a Mad Scientist in A Jet Boat',2006,1,NULL,3,'2.99',114,'21.99','PG-13','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(10,'ALADDIN CALENDAR','A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China',2006,1,NULL,6,'4.99',63,'24.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(11,'THE TEST I','Nothing happens',2006,1,NULL,6,'4.99',63,'24.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(12,'THE TEST II','Nothing happens again',2006,1,NULL,6,'4.99',63,'24.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(13,'THE TEST III','Nothing happens as usual',2006,1,NULL,6,'4.99',63,'24.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(14,'PAINT DRYING ON A WALL','Watch paint drying',2010,1,NULL,6,'4.99',63,'24.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'),
(15,'Melted','A Action-Packed Tale of a Man And a Lumberjack who must Reach a Feminist in Ancient China',2010,1,NULL,6,'4.99',63,'24.99','NC-17','Trailers,Deleted Scenes','2006-02-15 05:03:42'))*",

    R"*(INSERT INTO film_actor VALUES (1,1,'2006-02-15 05:05:03'),
(1,3,'2006-02-15 05:05:03'),
(1,5,'2006-02-15 05:05:03'),
(1,10,'2006-02-15 05:05:03'),
(2,3,'2006-02-15 05:05:03'),
(2,4,'2006-02-15 05:05:03'),
(2,8,'2006-02-15 05:05:03'),
(2,9,'2006-02-15 05:05:03'),
(3,1,'2006-02-15 05:05:03'),
(3,4,'2006-02-15 05:05:03'),
(4,8,'2006-02-15 05:05:03'),
(4,5,'2006-02-15 05:05:03'),
(5,3,'2006-02-15 05:05:03'),
(6,5,'2006-02-15 05:05:03'),
(6,7,'2006-02-15 05:05:03'),
(7,2,'2006-02-15 05:05:03'),
(7,9,'2006-02-15 05:05:03'),
(8,8,'2006-02-15 05:05:03'),
(8,9,'2006-02-15 05:05:03'),
(9,9,'2006-02-15 05:05:03'),
(9,10,'2006-02-15 05:05:03'),
(10,1,'2006-02-15 05:05:03'),
(10,9,'2006-02-15 05:05:03'),
(11,11,'2010-02-15 01:01:01'),
(11,12,'2010-02-15 01:01:01'),
(11,13,'2010-02-15 01:01:01'))*",

    R"*(INSERT INTO film_category VALUES (1,6,'2006-02-15 05:07:09'),
(2,5,'2006-02-15 05:07:09'),
(2,8,'2006-02-15 05:07:09'),
(2,11,'2006-02-15 05:07:09'),
(3,6,'2006-02-15 05:07:09'),
(3,7,'2006-02-15 05:07:09'),
(4,11,'2006-02-15 05:07:09'),
(5,8,'2006-02-15 05:07:09'),
(6,9,'2006-02-15 05:07:09'),
(7,5,'2006-02-15 05:07:09'),
(8,11,'2006-02-15 05:07:09'),
(9,11,'2006-02-15 05:07:09'),
(10,15,'2006-02-15 05:07:09'))*",

    // UUID PKs
    R"*(CREATE TABLE t1_owner (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(32)
  ))*",

    R"*(INSERT INTO t1_owner VALUES (0x75756964310000000000000000000000, 'one'), 
                    (0x75756964320000000000000000000000, 'two'))*",

    R"*(CREATE TABLE t1_ref_11 (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30)
    ))*",

    R"*(CREATE TABLE t1_base (
      id BINARY(16) PRIMARY KEY,
      owner_id BINARY(16),
      ref_11_id BINARY(16),
      data TEXT,
      FOREIGN KEY (ref_11_id) REFERENCES t1_ref_11 (id)
  ))*",

    R"*(CREATE TABLE t1_ref_1n (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30),
      base_id BINARY(16),
      FOREIGN KEY (base_id) REFERENCES t1_base (id)
    ))*",

    R"*(INSERT INTO t1_ref_11 VALUES ('UUID1', 'DATA1'))*",

    // AUTO_INC PKs
    R"*(CREATE TABLE t2_ref_11_11 (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30)
    ))*",

    R"*(CREATE TABLE t2_ref_11 (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      ref_id INT,
      FOREIGN KEY (ref_id) REFERENCES t2_ref_11_11 (id)
    ))*",

    R"*(CREATE TABLE t2_base (
      id INT PRIMARY KEY AUTO_INCREMENT,
      owner_id BINARY(16),
      ref_11_id INT,
      data1 TEXT,
      data2 INT,
      FOREIGN KEY (ref_11_id) REFERENCES t2_ref_11 (id)
  ))*",

    R"*(CREATE TABLE t2_ref_1n (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      base_id INT,
      FOREIGN KEY (base_id) REFERENCES t2_base (id)
    ))*",

    R"*(CREATE TABLE t2_ref_1n_1n (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      ref_1n_id INT,
      FOREIGN KEY (ref_1n_id) REFERENCES t2_ref_1n (id)
    ))*",

    R"*(CREATE TABLE t2_ref_nm (
      id INT AUTO_INCREMENT,
      data VARCHAR(30),
      PRIMARY KEY (id)
    ))*",

    R"*(CREATE TABLE t2_ref_nm_join (
      base_id INT,      
      ref_id INT,

      PRIMARY KEY (base_id, ref_id),
      FOREIGN KEY (base_id) REFERENCES t2_base (id),
      FOREIGN KEY (ref_id) REFERENCES t2_ref_nm (id)
    ))*",

    R"*(INSERT INTO t2_ref_11_11 VALUES (10, 'abc-1'), (11, 'abc-2'))*",

    R"*(INSERT INTO t2_ref_11 VALUES (20, 'ref11-1', NULL), (21, 'ref11-2', 10))*",

    R"*(INSERT INTO t2_base VALUES (1, 0x11110000000000000000000000000000, NULL, 'data1', 1),
     (2, 0x11110000000000000000000000000000, NULL, 'data2', 2), (3, 0x22220000000000000000000000000000, NULL, 'data3', 3),
     (4, 0x33330000000000000000000000000000, NULL, 'data4', 1), (5, 0x11110000000000000000000000000000, NULL, 'data5', 1),
     (6, 0x22220000000000000000000000000000, NULL, 'data6', 6), (7, 0x11110000000000000000000000000000, NULL, 'data1', 7),
     (9, 0x11110000000000000000000000000000, 21, 'hello', 1234))*",

    R"*(INSERT INTO t2_ref_nm VALUES (1, 'DATA1'), (2, 'DATA2'), (3, 'DATA3'))*",

    R"*(INSERT INTO t2_ref_nm_join VALUES (1, 2), (5, 1), (5, 3))*",

    // AUTO_INC, UUID PKs
    R"*(CREATE TABLE t3_ref_11 (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30)
    ))*",

    R"*(CREATE TABLE t3_base (
      id INT PRIMARY KEY AUTO_INCREMENT,
      owner_id INT,
      ref_11_id BINARY(16),
      data1 TEXT,
      data2 INT,
      FOREIGN KEY (ref_11_id) REFERENCES t3_ref_11 (id)
  ))*",

    R"*(CREATE TABLE t3_ref_1n (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30),
      base_id INT,
      FOREIGN KEY (base_id) REFERENCES t3_base (id)
    ))*",

    // UUID, AUTO_INC PKs
    R"*(CREATE TABLE t4_ref_11 (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30)
    ))*",

    R"*(CREATE TABLE t4_base (
      id BINARY(16) PRIMARY KEY,
      owner_id BINARY(16),
      ref_11_id INT,
      data TEXT,
      FOREIGN KEY (ref_11_id) REFERENCES t4_ref_11 (id)
  ))*",

    R"*(CREATE TABLE t4_ref_1n (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      base_id BINARY(16),
      FOREIGN KEY (base_id) REFERENCES t4_base (id)
    ))*",

    // AUTO_INC composite PKs
    R"*(CREATE TABLE tc2_ref_11 (
      id INT AUTO_INCREMENT,
      sub_id CHAR(3),
      data VARCHAR(30),
      PRIMARY KEY (id, sub_id)
    ))*",

    R"*(CREATE TABLE tc2_base (
      id INT AUTO_INCREMENT,
      sub_id CHAR(2),
      owner_id INT,
      ref_11_id INT,
      ref_11_sub_id CHAR(3),
      data1 TEXT,
      data2 INT,
      PRIMARY KEY (id, sub_id),
      FOREIGN KEY (ref_11_id, ref_11_sub_id) REFERENCES tc2_ref_11 (id, sub_id)
  ))*",

    R"*(INSERT INTO tc2_ref_11 VALUES 
      (1, 'AA', 'REF1'),
      (100, 'AA', 'REF2'),
      (101, 'AA', 'REF3'))*",

    R"*(INSERT INTO tc2_base VALUES (1, 'AA', NULL, NULL, NULL, 'AAA', 111),
    (2, 'BB', NULL, NULL, NULL, 'BBB', 222),
    (3, 'AA', NULL, NULL, NULL, 'AAA2', 333),
    (4, 'AA', NULL, NULL, NULL, 'CCC', 0),
    (5, 'AA', NULL, 100, 'AA', 'TEST', 0),
    (6, 'AA', NULL, 101, 'AA', 'TEST2', 0))*",

    R"*(CREATE TABLE tc2_ref_1n (
      id INT AUTO_INCREMENT,
      sub_id INT,
      data VARCHAR(30),
      base_id INT,
      base_sub_id CHAR(2),
      PRIMARY KEY (id, sub_id),
      FOREIGN KEY (base_id, base_sub_id) REFERENCES tc2_base (id, sub_id)
    ))*",

    R"*(INSERT INTO tc2_ref_1n VALUES (1, 1, 'data1', 2, 'BB'),
      (2, 2, 'data2', 1, 'AA'),(3, 1, 'data3', 1, 'AA'))*",

    R"*(CREATE TABLE tc2_ref_nm (
      id INT AUTO_INCREMENT,
      sub_id INT,
      data VARCHAR(30),
      PRIMARY KEY (id, sub_id)
    ))*",

    R"*(CREATE TABLE tc2_ref_nm_join (
      base_id INT,
      base_sub_id CHAR(2),
      
      ref_id INT,
      ref_sub_id INT,

      PRIMARY KEY (base_id, base_sub_id, ref_id, ref_sub_id),
      FOREIGN KEY (base_id, base_sub_id) REFERENCES tc2_base (id, sub_id),
      FOREIGN KEY (ref_id, ref_sub_id) REFERENCES tc2_ref_nm (id, sub_id)
    ))*",

    R"*(INSERT INTO tc2_ref_nm VALUES 
      (111, 888, 'Data1'),
      (222, 999, 'Data2'),
      (333, 777, 'Data3'))*",

    R"*(INSERT INTO tc2_ref_nm_join VALUES 
      (1, 'AA', 111, 888),
      (2, 'BB', 222, 999),
      (1, 'AA', 333, 777))*",

    // AUTO_INC composite/sharded PKs
    R"*(CREATE TABLE ts2_ref_11 (
      id INT AUTO_INCREMENT,
      data VARCHAR(30),
      shard_id INT,
      PRIMARY KEY (id, shard_id)
    ))*",

    R"*(CREATE TABLE ts2_base (
      id INT AUTO_INCREMENT,
      shard_id INT,
      owner_id INT,
      ref_11_id INT,
      data1 TEXT,
      data2 INT,
      PRIMARY KEY (id, shard_id),
      FOREIGN KEY (ref_11_id, shard_id) REFERENCES ts2_ref_11 (id, shard_id)
  ))*",

    R"*(INSERT INTO ts2_base VALUES (1, 91, NULL, NULL, 'AAA', 111),
    (2, 92, NULL, NULL, 'BBB', 222),
    (3, 91, NULL, NULL, 'AAA2', 333))*",

    R"*(CREATE TABLE ts2_ref_1n (
      id INT AUTO_INCREMENT,
      shard_id INT,
      data VARCHAR(30),
      base_id INT,
      PRIMARY KEY (id, shard_id),
      FOREIGN KEY (base_id, shard_id) REFERENCES ts2_base (id, shard_id)
    ))*",

    R"*(INSERT INTO ts2_ref_1n VALUES (1, 91, 'data1', 1),
      (2, 92, 'data2', 2), (3, 91, 'data3', 3))*",

    R"*(CREATE TABLE ts2_ref_nm (
      id INT AUTO_INCREMENT,
      shard_id INT,
      data VARCHAR(30),
      PRIMARY KEY (id, shard_id)
    ))*",

    R"*(CREATE TABLE ts2_ref_nm_join (
      shard_id INT,
      base_id INT,
      
      ref_id INT,

      PRIMARY KEY (shard_id, base_id, ref_id),
      FOREIGN KEY (base_id, shard_id) REFERENCES ts2_base (id, shard_id),
      FOREIGN KEY (ref_id, shard_id) REFERENCES ts2_ref_nm (id, shard_id)
    ))*",

    R"*(INSERT INTO ts2_ref_nm VALUES 
      (11, 91, 'Data1'),
      (12, 92, 'Data2'),
      (13, 91, 'Data3'))*",

    R"*(INSERT INTO ts2_ref_nm_join VALUES 
      (91, 1, 11),
      (92, 2, 12),
      (91, 1, 13))*"

};

void DatabaseRestTableTest::SetUp() {
  m_ = std::make_unique<mysqlrouter::MySQLSession>();
  m_->connect("localhost", 3306, "root", "", "", "",
              mysqlrouter::MySQLSession::kDefaultConnectTimeout,
              mysqlrouter::MySQLSession::kDefaultReadTimeout,
              CLIENT_FOUND_ROWS);

  reset_test();
}

void DatabaseRestTableTest::TearDown() {
  if (getenv("SKIP_TEARDOWN")) drop_schema();
}

std::string DatabaseRestTableTest::select_one(
    std::shared_ptr<mrs::database::entry::DualityView> view,
    const mrs::database::PrimaryKeyColumnValues &pk,
    const mrs::database::dv::ObjectFieldFilter &field_filter,
    const mrs::database::ObjectRowOwnership &row_owner, bool compute_etag) {
  mrs::database::QueryRestTableSingleRow rest{nullptr, false,
                                              select_include_links_};

  rest.query_entry(m_.get(), view, pk, field_filter, "localhost", row_owner,
                   compute_etag);

  return rest.response;
}

void DatabaseRestTableTest::reset_test() {
  drop_schema();
  create_schema();

  snapshot();
}

void DatabaseRestTableTest::snapshot() {
  // count initial size of tables
  initial_table_sizes_.clear();
  m_->query("SHOW TABLES IN mrstestdb", [this](const auto &row) {
    initial_table_sizes_[row[0]] = 0;
    return true;
  });

  for (auto &t : initial_table_sizes_) {
    t.second =
        atoi((*m_->query_one("SELECT COUNT(*) FROM mrstestdb." + t.first))[0]);
  }

  auto row = m_->query_one("SHOW BINARY LOG STATUS");
  initial_binlog_file_ = (*row)[0];
  initial_binlog_position_ = std::stoull((*row)[1]);
}

void DatabaseRestTableTest::expect_rows_added(
    const std::map<std::string, int> &changes) {
  assert(!initial_table_sizes_.empty());

  std::map<std::string, int> current_sizes;

  m_->query("SHOW TABLES IN mrstestdb", [&](const auto &row) {
    current_sizes[row[0]] = 0;
    return true;
  });
  for (auto &t : current_sizes) {
    t.second =
        atoi((*m_->query_one("SELECT COUNT(*) FROM mrstestdb." + t.first))[0]);
  }

  auto rows_added = [&](const std::string &table) {
    return current_sizes.at(table) - initial_table_sizes_.at(table);
  };

  for (const auto &ch : changes) {
    if (current_sizes.find(ch.first) == changes.end())
      throw std::invalid_argument("invalid table " + ch.first);
  }

  for (const auto &ch : current_sizes) {
    if (changes.find(ch.first) == changes.end())
      EXPECT_EQ(rows_added(ch.first), 0)
          << "Unexpected changes to table: " + ch.first;
    else
      EXPECT_EQ(rows_added(ch.first), changes.at(ch.first))
          << "Unexpected number of rows in table: " + ch.first;
  }
}

void DatabaseRestTableTest::create_schema() {
  for (const char *sql : k_test_ddl) {
    m_->execute(sql);
  }
}

void DatabaseRestTableTest::drop_schema() {
  m_->execute("DROP SCHEMA IF EXISTS mrstestdb");
}

void DatabaseRestTableTest::prepare(TestSchema schema) {
  const std::map<TestSchema, std::vector<const char *>> k_sql = {
      {TestSchema::PLAIN,
       {// plain PKs
        R"*(CREATE TABLE child_11 (
      id INT PRIMARY KEY,
      data VARCHAR(30)
    ))*",

        R"*(CREATE TABLE root_owner (
      id BINARY(16) PRIMARY KEY,
      child_11_id INT,
      data1 TEXT,
      data2 INT,
      FOREIGN KEY (child_11_id) REFERENCES child_11 (id)
  ))*",

        R"*(CREATE TABLE root (
      id INT PRIMARY KEY,
      owner_id BINARY(16),
      child_11_id INT,
      data1 TEXT,
      data2 INT,
      FOREIGN KEY (child_11_id) REFERENCES child_11 (id)
  ))*",

        R"*(CREATE TABLE child_1n (
      id INT PRIMARY KEY,
      data VARCHAR(30),
      root_id INT,
      FOREIGN KEY (root_id) REFERENCES root (id)
    ))*",

        R"*(CREATE TABLE child_nm (
      id INT,
      data VARCHAR(30),
      PRIMARY KEY (id)
    ))*",

        R"*(CREATE TABLE child_nm_join (
      root_id INT,      
      child_id INT,

      PRIMARY KEY (root_id, child_id),
      FOREIGN KEY (root_id) REFERENCES root (id),
      FOREIGN KEY (child_id) REFERENCES child_nm (id)
    ))*",

        R"*(CREATE TABLE child_nm_join2 (
      root_id BINARY(16),      
      child_id INT,

      PRIMARY KEY (root_id, child_id),
      FOREIGN KEY (root_id) REFERENCES root_owner (id),
      FOREIGN KEY (child_id) REFERENCES child_nm (id)
    ))*",

        R"*(INSERT INTO child_11 VALUES (20, 'ref11-1'), (21, 'ref11-2'), (22, 'ref11-3'))*",
        R"*(INSERT INTO root VALUES (1, 0x11110000000000000000000000000000, NULL, 'data1', 1),
     (2, 0x11110000000000000000000000000000, NULL, 'data2', 2), (3, 0x22220000000000000000000000000000, NULL, 'data3', 3),
     (4, 0x33330000000000000000000000000000, NULL, 'data4', 1), (5, 0x11110000000000000000000000000000, NULL, 'data5', 1),
     (6, 0x22220000000000000000000000000000, NULL, 'data6', 6), (7, 0x11110000000000000000000000000000, NULL, 'data1', 7),
     (9, 0x11110000000000000000000000000000, 21, 'hello', 1234), (10, 0x33330000000000000000000000000000, null, 'data2', 2))*",
        R"*(INSERT INTO child_1n VALUES (1, 'ref1n-1', 1), (2, 'ref1n-2', 1),
        (3, 'ref1n-3', 4),
        (10, 'test child1', 10), (11, 'test child2', 10))*",

        R"*(INSERT INTO child_nm VALUES (1, 'one'), (2, 'two'), (3, 'three'))*"}},

      {TestSchema::AUTO_INC,
       {// AUTO_INC PKs
        R"*(CREATE TABLE child_11_11 (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30)
    ))*",

        R"*(CREATE TABLE child_11 (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      child_11_11_id INT,
      FOREIGN KEY (child_11_11_id) REFERENCES child_11_11 (id)
    ))*",

        R"*(CREATE TABLE root (
      id INT PRIMARY KEY AUTO_INCREMENT,
      owner_id BINARY(16),
      child_11_id INT,
      data1 TEXT,
      data2 INT,
      FOREIGN KEY (child_11_id) REFERENCES child_11 (id)
  ))*",

        R"*(CREATE TABLE child_1n (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      root_id INT,
      FOREIGN KEY (root_id) REFERENCES root (id)
    ))*",

        R"*(CREATE TABLE child_1n_1n (
      id INT PRIMARY KEY AUTO_INCREMENT,
      data VARCHAR(30),
      child_1n_id INT,
      FOREIGN KEY (child_1n_id) REFERENCES child_1n (id)
    ))*",

        R"*(CREATE TABLE child_nm (
      id INT AUTO_INCREMENT,
      data VARCHAR(30),
      PRIMARY KEY (id)
    ))*",

        R"*(CREATE TABLE child_nm_join (
      root_id INT,      
      child_id INT,

      PRIMARY KEY (root_id, child_id),
      FOREIGN KEY (root_id) REFERENCES root (id),
      FOREIGN KEY (child_id) REFERENCES child_nm (id)
    ))*",

        R"*(INSERT INTO child_11_11 VALUES (10, 'abc-1'), (11, 'abc-2'))*",

        R"*(INSERT INTO child_11 VALUES (20, 'ref11-1', NULL), (21, 'ref11-2', 10))*",

        R"*(INSERT INTO root VALUES (1, 0x11110000000000000000000000000000, NULL, 'data1', 1),
     (2, 0x11110000000000000000000000000000, NULL, 'data2', 2), (3, 0x22220000000000000000000000000000, NULL, 'data3', 3),
     (4, 0x33330000000000000000000000000000, NULL, 'data4', 1), (5, 0x11110000000000000000000000000000, NULL, 'data5', 1),
     (6, 0x22220000000000000000000000000000, NULL, 'data6', 6), (7, 0x11110000000000000000000000000000, NULL, 'data1', 7),
     (9, 0x11110000000000000000000000000000, 21, 'hello', 1234), (10, 0x33330000000000000000000000000000, null, 'data1', 42))*",

        R"*(INSERT INTO child_1n VALUES (1, 'ref1n-1', 1), (2, 'ref1n-2', 1),
        (3, 'ref1n-3', 4),
        (4, 'ref1n-4', 9), (5, 'ref1n-5', 9), (6, 'ref1n-6', 9),
        (10, 'test child1', 10), (11, 'test child2', 10))*",

        R"*(INSERT INTO child_1n_1n VALUES (30, '1n1n-1', 4), (31, '1n1n-2', 4), (32, '1n1n-3', 6))*",

        R"*(INSERT INTO child_nm VALUES (1, 'DATA1'), (2, 'DATA2'), (3, 'DATA3'))*",

        R"*(INSERT INTO child_nm_join VALUES (1, 2), (5, 1), (5, 3),
            (9, 2), (9, 3))*"}},

      {TestSchema::UUID,
       {// UUID PKs
        R"*(CREATE TABLE owner (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(32)
  ))*",

        R"*(INSERT INTO owner VALUES (0x111, 'one'), (0x222, 'two'))*",

        R"*(CREATE TABLE child_11_11 (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30)
    ))*",

        R"*(CREATE TABLE child_11 (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30),
      child_11_11_id BINARY(16),
      FOREIGN KEY (child_11_11_id) REFERENCES child_11_11 (id)
    ))*",

        R"*(CREATE TABLE root (
      id BINARY(16) PRIMARY KEY,
      owner_id BINARY(16),
      child_11_id BINARY(16),
      data1 TEXT,
      data2 INT,
      FOREIGN KEY (child_11_id) REFERENCES child_11 (id)
  ))*",

        R"*(CREATE TABLE child_1n (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30),
      root_id BINARY(16),
      FOREIGN KEY (root_id) REFERENCES root (id)
    ))*",

        R"*(CREATE TABLE child_1n_1n (
      id BINARY(16) PRIMARY KEY,
      data VARCHAR(30),
      child_1n_id BINARY(16),
      FOREIGN KEY (child_1n_id) REFERENCES child_1n (id)
    ))*",

        R"*(CREATE TABLE child_nm (
      id BINARY(16) ,
      data VARCHAR(30),
      PRIMARY KEY (id)
    ))*",

        R"*(CREATE TABLE child_nm_join (
      root_id BINARY(16),      
      child_id BINARY(16),

      PRIMARY KEY (root_id, child_id),
      FOREIGN KEY (root_id) REFERENCES root (id),
      FOREIGN KEY (child_id) REFERENCES child_nm (id)
    ))*",

        R"*(INSERT INTO child_11_11 VALUES (0x10, 'abc-1'), (0x11, 'abc-2'))*",

        R"*(INSERT INTO child_11 VALUES (0x20, 'ref11-1', NULL), (0x21, 'ref11-2', 0x10))*",

        R"*(INSERT INTO root VALUES (0x1, 0x111, NULL, 'data1', 1),
     (0x2, 0x111, NULL, 'data2', 2), (0x3, 0x222, NULL, 'data3', 3),
     (0x4, 0x333, NULL, 'data4', 1), (0x5, 0x111, NULL, 'data5', 1),
     (0x6, 0x222, NULL, 'data6', 6), (0x7, 0x111, NULL, 'data1', 7),
     (0x9, 0x111, 0x21, 'hello', 1234))*",

        R"*(INSERT INTO child_1n VALUES (0x30, 'ref1n-1', NULL), (0x31, 'ref1n-2', 0x1))*",

        R"*(INSERT INTO child_nm VALUES (0x1, 'DATA1'), (0x2, 'DATA2'), (0x3, 'DATA3'))*",

        R"*(INSERT INTO child_nm_join VALUES (0x1, 0x2), (0x5, 0x1), (0x5, 0x3))*"}},

      {TestSchema::MULTI, {R"*(CREATE TABLE root (
      id1 INT AUTO_INCREMENT,
      id2 BINARY(16),
      data1 TEXT,
      data2 INT,
      PRIMARY KEY(id1, id2)
  ))*"}}};

  m_->execute("create schema if not exists mrstestdb");
  m_->execute("use mrstestdb");

  for (const char *sql : k_sql.at(schema)) {
    m_->execute(sql);
  }
}

int DatabaseRestTableTest::num_rows_added(const std::string &table) {
  auto num_rows =
      atoi((*m_->query_one("SELECT COUNT(*) FROM mrstestdb." + table))[0]);
  return num_rows - initial_table_sizes_[table];
}

bool DatabaseRestTableTest::binlog_changed() const {
  auto row = m_->query_one("SHOW BINARY LOG STATUS");
  if (initial_binlog_file_ != (*row)[0]) return true;

  if (initial_binlog_position_ != std::stoull((*row)[1])) return true;

  return false;
}

void DatabaseRestTableTest::prepare_user_metadata() {
  // This should match the latest version of mrs_metadata_schema.sql (with some
  // FKs to unused tables removed)
  // Use of this should be minimized in unit-tests, most tests that need the MD
  // should be done in MTR

  static std::vector<const char *> k_sql = {
      "DROP SCHEMA IF EXISTS mysql_rest_service_metadata",
      "CREATE SCHEMA mysql_rest_service_metadata",

      R"*(CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user` (
  `id` BINARY(16) NOT NULL,
  `auth_app_id` BINARY(16) NOT NULL,
  `name` VARCHAR(225) NULL,
  `email` VARCHAR(255) NULL,
  `vendor_user_id` VARCHAR(255) NULL,
  `login_permitted` TINYINT NOT NULL DEFAULT 0,
  `mapped_user_id` VARCHAR(255) NULL,
  `app_options` JSON NULL,
  `auth_string` TEXT NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_auth_user_auth_app1_idx` (`auth_app_id` ASC) VISIBLE
) ENGINE = InnoDB;)*",

      R"*(CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_hierarchy` (
  `user_id` BINARY(16) NOT NULL,
  `reporting_to_user_id` BINARY(16) NOT NULL,
  `user_hierarchy_type_id` BINARY(16) NOT NULL,
  `options` JSON NULL,
  PRIMARY KEY (`user_id`, `reporting_to_user_id`, `user_hierarchy_type_id`),
  INDEX `fk_user_hierarchy_auth_user2_idx` (`reporting_to_user_id` ASC) VISIBLE,
  INDEX `fk_user_hierarchy_hierarchy_type1_idx` (`user_hierarchy_type_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_hierarchy_auth_user1`
    FOREIGN KEY (`user_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_hierarchy_auth_user2`
    FOREIGN KEY (`reporting_to_user_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
) ENGINE = InnoDB;)*",

      R"*(CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_group` (
  `id` BINARY(16) NOT NULL,
  `specific_to_service_id` BINARY(16) NULL,
  `caption` VARCHAR(45) NULL,
  `description` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_user_group_service1_idx` (`specific_to_service_id` ASC) VISIBLE
) ENGINE = InnoDB;)*",

      R"*(CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_group_hierarchy` (
  `user_group_id` BINARY(16) NOT NULL,
  `parent_group_id` BINARY(16) NOT NULL,
  `group_hierarchy_type_id` BINARY(16) NOT NULL,
  `level` INT UNSIGNED NOT NULL DEFAULT 0,
  `options` JSON NULL,
  PRIMARY KEY (`user_group_id`, `parent_group_id`, `group_hierarchy_type_id`),
  INDEX `fk_user_group_has_user_group_user_group2_idx` (`parent_group_id` ASC) VISIBLE,
  INDEX `fk_user_group_has_user_group_user_group1_idx` (`user_group_id` ASC) VISIBLE,
  INDEX `fk_user_group_hierarchy_group_hierarchy_type1_idx` (`group_hierarchy_type_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_group_has_user_group_user_group1`
    FOREIGN KEY (`user_group_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_group` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_group_has_user_group_user_group2`
    FOREIGN KEY (`parent_group_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_group` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
) ENGINE = InnoDB;)*",

      R"*(INSERT INTO mysql_rest_service_metadata.mrs_user
          (id, name, auth_app_id) VALUES
          (0x11110000000000000000000000000000, 'UserOne', 0),
          (0x22220000000000000000000000000000, 'UserTwo', 0),
          (0x33330000000000000000000000000000, 'UserThree', 0))*",
  };

  for (const char *sql : k_sql) {
    m_->execute(sql);
  }
}
