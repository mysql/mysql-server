# Case 1 : CREATE FULLTEXT/ID + TRUNCATE
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT fidx(title, body)
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
TRUNCATE TABLE articles;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
ALTER TABLE articles DROP KEY fidx;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
TRUNCATE TABLE articles;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE articles ADD FULLTEXT KEY fidx(title, body);
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
# Case 2 : CREATE FULLTEXT/ID + ALTER +  TRUNCATE
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT
) ENGINE=InnoDB;
ALTER TABLE articles ADD FULLTEXT KEY fidx(title, body);
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
TRUNCATE TABLE articles;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
ALTER TABLE articles DROP KEY fidx;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
TRUNCATE TABLE articles;
# restart
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE articles ADD FULLTEXT KEY fidx(title, body);
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
# Case 3 : CREATE FULLTEXT + TRUNCATE
CREATE TABLE articles (
title VARCHAR(200),
body TEXT,
FULLTEXT fidx(title, body)
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
TRUNCATE TABLE articles;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
ALTER TABLE articles DROP KEY fidx;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
TRUNCATE TABLE articles;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE articles ADD FULLTEXT KEY fidx(title, body);
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
# Case 4 : CREATE FULLTEXT + ALTER + TRUNCATE
CREATE TABLE articles (
title VARCHAR(200),
body TEXT
) ENGINE=InnoDB;
ALTER TABLE articles ADD FULLTEXT KEY fidx(title, body);
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
TRUNCATE TABLE articles;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
ALTER TABLE articles DROP KEY fidx;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
TRUNCATE TABLE articles;
# restart
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE articles ADD FULLTEXT KEY fidx(title, body);
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...');
SELECT title, body FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
title	body
MySQL Tutorial	DBMS stands for MySQL DataBase ...
MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
