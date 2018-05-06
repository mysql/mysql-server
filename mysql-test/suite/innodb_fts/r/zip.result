CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body)
) ENGINE=InnoDB ROW_FORMAT=compressed KEY_BLOCK_SIZE=2;
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
