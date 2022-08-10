CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB;
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT create_options FROM information_schema.tables
WHERE table_name='test_ps_sample_pages';
CREATE_OPTIONS	
ALTER TABLE test_ps_sample_pages STATS_SAMPLE_PAGES=12345;
# restart
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci STATS_SAMPLE_PAGES=12345
SELECT create_options FROM information_schema.tables
WHERE table_name='test_ps_sample_pages';
CREATE_OPTIONS	stats_sample_pages=12345
DROP TABLE test_ps_sample_pages;
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=default;
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT create_options FROM information_schema.tables
WHERE table_name='test_ps_sample_pages';
CREATE_OPTIONS	
DROP TABLE test_ps_sample_pages;
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=-5;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-5' at line 2
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=0;
ERROR 42000: The valid range for stats_sample_pages is [1, 65535]. Error near '0' at line 2
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=67000;
ERROR 42000: The valid range for stats_sample_pages is [1, 65535]. Error near '67000' at line 2
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=670000;
ERROR 42000: The valid range for stats_sample_pages is [1, 65535]. Error near '670000' at line 2
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=65536;
ERROR 42000: The valid range for stats_sample_pages is [1, 65535]. Error near '65536' at line 2
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=65535;
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci STATS_SAMPLE_PAGES=65535
DROP TABLE test_ps_sample_pages;
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=1;
# restart
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci STATS_SAMPLE_PAGES=1
SELECT create_options FROM information_schema.tables
WHERE table_name='test_ps_sample_pages';
CREATE_OPTIONS	stats_sample_pages=1
DROP TABLE test_ps_sample_pages;
CREATE TABLE test_ps_sample_pages (a INT, PRIMARY KEY (a)) ENGINE=INNODB
STATS_SAMPLE_PAGES=5678;
# restart
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci STATS_SAMPLE_PAGES=5678
SELECT create_options FROM information_schema.tables
WHERE table_name='test_ps_sample_pages';
CREATE_OPTIONS	stats_sample_pages=5678
ALTER TABLE test_ps_sample_pages STATS_SAMPLE_PAGES=default;
# restart
SHOW CREATE TABLE test_ps_sample_pages;
Table	test_ps_sample_pages
Create Table	CREATE TABLE `test_ps_sample_pages` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT create_options FROM information_schema.tables
WHERE table_name='test_ps_sample_pages';
CREATE_OPTIONS	
DROP TABLE test_ps_sample_pages;
