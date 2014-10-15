CREATE TABLE articles (
id INT UNSIGNED NOT NULL PRIMARY KEY,
title VARCHAR(200),
FULLTEXT (title)
) ENGINE= InnoDB;
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
4	mysql
5	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
ROLLBACK TO SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
4	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
5	mysql
6	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
ROLLBACK TO SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
3	mysql
4	mysql
5	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
RELEASE SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
3	mysql
4	mysql
5	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
3	mysql
4	mysql
5	mysql
6	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
RELEASE SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
3	mysql
4	mysql
5	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
RELEASE SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
4	mysql
5	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
ROLLBACK TO SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
3	mysql
4	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
RELEASE SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
ROLLBACK TO SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
COMMIT;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
2	mysql
3	mysql
5	mysql
6	mysql
7	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
ROLLBACK;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
3	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
ROLLBACK;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
4	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
ROLLBACK;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
5	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
ROLLBACK TO SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
ROLLBACK;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
5	mysql
TRUNCATE TABLE articles;
INSERT INTO articles(id, title) VALUES(1, 'mysql');
BEGIN;
INSERT INTO articles(id, title) VALUES(2, 'mysql');
SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(3, 'mysql');
SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(4, 'mysql');
ROLLBACK TO SAVEPOINT sp2;
INSERT INTO articles(id, title) VALUES(5, 'mysql');
RELEASE SAVEPOINT sp1;
INSERT INTO articles(id, title) VALUES(6, 'mysql');
ROLLBACK;
INSERT INTO articles(id, title) VALUES(7, 'mysql');
SELECT * FROM articles WHERE MATCH(title) AGAINST('mysql');
id	title
1	mysql
7	mysql
DROP TABLE articles;
