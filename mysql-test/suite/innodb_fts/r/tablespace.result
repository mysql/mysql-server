Test Case 1 : System Tablespace
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body)
) ENGINE=InnoDB tablespace innodb_system;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
# restart
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
Test Case 2 : User-defined Tablespace
CREATE TABLESPACE s_def ADD DATAFILE 's_def.ibd';
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body)
) ENGINE=InnoDB tablespace s_def;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
# restart
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
DROP TABLESPACE s_def;
Test Case 3 : File-Per-Tablespace
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body)
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
# restart
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
