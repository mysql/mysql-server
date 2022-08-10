INSTALL PLUGIN simple_parser SONAME 'mypluglib.so';
# Test Part 2: Create Index Test(CREATE TABLE WITH FULLTEXT INDEX)
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
body1 TEXT,
body2 TEXT,
body3 TEXT,
FULLTEXT (title, body) WITH PARSER simple_parser,
FULLTEXT (title, body1) WITH PARSER simple_parser,
FULLTEXT (body3)
) ENGINE=InnoDB;
INSERT INTO articles (title, body,body1,body2,body3) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...','row1col4','row1col5','row1col5'),
('How To Use MySQL Well','After you went through a ...','row2col4','row2col5','row2col5'),
('Optimizing MySQL','In this tutorial we will show ...','row3col4','row3col5','row3col5 for'),
('1001 MySQL Tricks','How to use full-text search engine','row4col4','row4col5','row4col5'),
('Go MySQL Tricks','How to use full text search engine','row5col4 this','row5col5','row5col5');
ALTER TABLE articles ADD FULLTEXT INDEX (body1,body2) WITH PARSER simple_parser;
ALTER TABLE articles ADD FULLTEXT INDEX (body2,body3);
ALTER TABLE articles ADD FULLTEXT INDEX (body) WITH PARSER simple_parser;
ANALYZE TABLE articles;
Table	Op	Msg_type	Msg_text
test.articles	analyze	status	OK
SELECT
PLUGIN_NAME, PLUGIN_STATUS, PLUGIN_TYPE,
PLUGIN_LIBRARY
FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME LIKE '%simple_parser%';
PLUGIN_NAME	PLUGIN_STATUS	PLUGIN_TYPE	PLUGIN_LIBRARY
simple_parser	ACTIVE	FTPARSER	mypluglib.so
INSERT INTO articles (title, body,body1,body2,body3) VALUES (NULL,NULL,NULL,NULL,NULL);
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
2	How To Use MySQL Well	After you went through a ...	row2col4	row2col5	row2col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
4	1001 MySQL Tricks	How to use full-text search engine	row4col4	row4col5	row4col5
5	Go MySQL Tricks	How to use full text search engine	row5col4 this	row5col5	row5col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('dbms' WITH QUERY EXPANSION);
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
2	How To Use MySQL Well	After you went through a ...	row2col4	row2col5	row2col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
4	1001 MySQL Tricks	How to use full-text search engine	row4col4	row4col5	row4col5
5	Go MySQL Tricks	How to use full text search engine	row5col4 this	row5col5	row5col5
SELECT * FROM articles WHERE
MATCH(title, body1) AGAINST('row1col4');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('row5col4' WITH QUERY EXPANSION);
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(body1, body2) AGAINST('row3col4');
id	title	body	body1	body2	body3
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
SELECT * FROM articles WHERE
MATCH(body) AGAINST('well' WITH QUERY EXPANSION);
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('will go');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('this');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('for');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(body1, body2) AGAINST('this');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(body) AGAINST('this');
id	title	body	body1	body2	body3
# Test plugin parser tokenizer difference
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full-text');
id	title	body	body1	body2	body3
4	1001 MySQL Tricks	How to use full-text search engine	row4col4	row4col5	row4col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full text');
id	title	body	body1	body2	body3
5	Go MySQL Tricks	How to use full text search engine	row5col4 this	row5col5	row5col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body	body1	body2	body3
START TRANSACTION;
INSERT INTO articles (title, body,body1,body2,body3) VALUES
('latest Tutorial','transactional properties tests ...','row6col4','row6col5','row6col5'),
('certification guide','all mysql exams tests ...','row7col4','row7col5','row7col5');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
2	How To Use MySQL Well	After you went through a ...	row2col4	row2col5	row2col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
4	1001 MySQL Tricks	How to use full-text search engine	row4col4	row4col5	row4col5
5	Go MySQL Tricks	How to use full text search engine	row5col4 this	row5col5	row5col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('Tutorial');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('guide');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('tests');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body1) AGAINST('row6col4');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(body1,body2) AGAINST('row7col5');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE id > 5;
id	title	body	body1	body2	body3
6	NULL	NULL	NULL	NULL	NULL
7	latest Tutorial	transactional properties tests ...	row6col4	row6col5	row6col5
8	certification guide	all mysql exams tests ...	row7col4	row7col5	row7col5
ROLLBACK;
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
2	How To Use MySQL Well	After you went through a ...	row2col4	row2col5	row2col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
4	1001 MySQL Tricks	How to use full-text search engine	row4col4	row4col5	row4col5
5	Go MySQL Tricks	How to use full text search engine	row5col4 this	row5col5	row5col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('Tutorial');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('guide');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('tests');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body1) AGAINST('row6col4');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(body1,body2) AGAINST('row7col5');
id	title	body	body1	body2	body3
START TRANSACTION;
INSERT INTO articles (title, body,body1,body2,body3) VALUES
('latest Tutorial','transactional properties tests ...','row6col4','row6col5','row6col5');
COMMIT;
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
2	How To Use MySQL Well	After you went through a ...	row2col4	row2col5	row2col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
4	1001 MySQL Tricks	How to use full-text search engine	row4col4	row4col5	row4col5
5	Go MySQL Tricks	How to use full text search engine	row5col4 this	row5col5	row5col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('Tutorial');
id	title	body	body1	body2	body3
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...	row1col4	row1col5	row1col5
3	Optimizing MySQL	In this tutorial we will show ...	row3col4	row3col5	row3col5 for
9	latest Tutorial	transactional properties tests ...	row6col4	row6col5	row6col5
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('guide');
id	title	body	body1	body2	body3
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('tests');
id	title	body	body1	body2	body3
9	latest Tutorial	transactional properties tests ...	row6col4	row6col5	row6col5
SELECT * FROM articles WHERE
MATCH(body) AGAINST('tests');
id	title	body	body1	body2	body3
9	latest Tutorial	transactional properties tests ...	row6col4	row6col5	row6col5
SELECT * FROM articles WHERE
MATCH(title, body1) AGAINST('row6col4');
id	title	body	body1	body2	body3
9	latest Tutorial	transactional properties tests ...	row6col4	row6col5	row6col5
SELECT * FROM articles WHERE
MATCH(body1,body2) AGAINST('row7col5');
id	title	body	body1	body2	body3
SELECT COUNT(*) FROM articles;
COUNT(*)
7
ALTER TABLE articles ADD COLUMN col_test VARCHAR(100) DEFAULT 'check add drop column';
SELECT COUNT(*) FROM articles WHERE col_test = 'check add drop column';
COUNT(*)
7
ALTER TABLE articles ADD FULLTEXT INDEX `idx_add` (col_test) WITH PARSER simple_parser;
INSERT INTO articles (title, body,body1,body2,body3,col_test) VALUES
('value','value','value','value','value','value');
SELECT COUNT(*) AS score FROM articles WHERE
MATCH(col_test) AGAINST('check');
score
7
SELECT COUNT(*) AS score FROM articles WHERE
MATCH(col_test) AGAINST('value');
score
1
ALTER TABLE articles DROP COLUMN col_test;
SELECT * FROM articles WHERE
MATCH(title, body1) AGAINST('row6col4');
id	title	body	body1	body2	body3
9	latest Tutorial	transactional properties tests ...	row6col4	row6col5	row6col5
SELECT * FROM articles WHERE
MATCH(body1,body2) AGAINST('row7col5');
id	title	body	body1	body2	body3
DROP TABLE articles;
SET NAMES UTF8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
a VARCHAR(200),
b TEXT,
FULLTEXT (b) WITH PARSER simple_parser
) CHARACTER SET = UTF8,ENGINE = InnoDB;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
INSERT INTO t1 (a,b) VALUES
('MySQL from Tutorial','DBMS stands for DataBase ...')  ,
('when To Use MySQL Well','After that you went through a ...'),
('where will Optimizing MySQL','what In this tutorial we will show ...'),
('Я могу есть стекло', 'оно мне Mне вредит'),
('Μπορῶ νὰ φάω σπασμένα' ,'γυαλιὰ χωρὶς νὰ πάθω τίποτα'),
('Sævör grét', 'áðan því úlpan var ónýt');
CREATE FULLTEXT INDEX idx on t1 (a,b) WITH PARSER simple_parser;
INSERT INTO t1 (a,b) VALUES
('adding record using session 1','for verifying multiple concurrent transactions'),
('Мога да ям стъкло', 'то Mне ми вреди');
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('+Mне' IN BOOLEAN MODE);
id	a	b
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('Mне');
id	a	b
4	Я могу есть стекло	оно мне Mне вредит
8	Мога да ям стъкло	то Mне ми вреди
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('могу');
id	a	b
4	Я могу есть стекло	оно мне Mне вредит
SELECT * FROM t1 WHERE MATCH (b) AGAINST ('áðan');
id	a	b
6	Sævör grét	áðan því úlpan var ónýt
SELECT * FROM t1 WHERE MATCH (b) AGAINST ('áðan') OR MATCH (a,b) AGAINST ('Mне могу');
id	a	b
4	Я могу есть стекло	оно мне Mне вредит
6	Sævör grét	áðan því úlpan var ónýt
8	Мога да ям стъкло	то Mне ми вреди
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('стъкло');
id	a	b
8	Мога да ям стъкло	то Mне ми вреди
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('"dbms stands"@05' IN BOOLEAN MODE);
id	a	b
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('for');
id	a	b
DROP TABLE t1;
SELECT @@innodb_ft_min_token_size,@@innodb_ft_max_token_size;
@@innodb_ft_min_token_size	@@innodb_ft_max_token_size
3	84
CREATE TABLE t1 (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
a VARCHAR(200),
b TEXT,
FULLTEXT (b) WITH PARSER simple_parser
) CHARACTER SET = UTF8,ENGINE = InnoDB;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
INSERT INTO t1 (a,b) VALUES
('MySQL from Tutorial','DBMS stands for DataBase ...')  ,
('when To Use MySQL Well','After that you went through a ...'),
('where will Optimizing MySQL','what In this tutorial we will show ...'),
('Я могу есть стекло', 'оно мне Mне вредит'),
('Μπορῶ νὰ φάω σπασμένα' ,'γυαλιὰ χωρὶς νὰ πάθω τίποτα'),
('Sævör grét', 'áðan því úlpan var ónýt');
CREATE FULLTEXT INDEX idx on t1 (a,b) WITH PARSER simple_parser;
INSERT INTO t1 (a,b) VALUES
('adding record using session 1','for verifying multiple concurrent transactions'),
('Мога да ям стъкло', 'то Mне ми вреди'),(NULL,NULL);
# restart:--innodb_ft_min_token_size=5 --innodb_ft_max_token_size=10
SELECT @@innodb_ft_min_token_size,@@innodb_ft_max_token_size;
@@innodb_ft_min_token_size	@@innodb_ft_max_token_size
5	10
ALTER TABLE t1 DROP INDEX idx;
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('+MySQL' IN BOOLEAN MODE);
ERROR HY000: Can't find FULLTEXT index matching the column list
CREATE FULLTEXT INDEX idx on t1 (a,b) WITH PARSER simple_parser;
ALTER TABLE t1 DROP INDEX idx;
CREATE FULLTEXT INDEX idx on t1 (a,b);
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('+MySQL' IN BOOLEAN MODE);
id	a	b
1	MySQL from Tutorial	DBMS stands for DataBase ...
2	when To Use MySQL Well	After that you went through a ...
3	where will Optimizing MySQL	what In this tutorial we will show ...
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('MySQL');
id	a	b
1	MySQL from Tutorial	DBMS stands for DataBase ...
2	when To Use MySQL Well	After that you went through a ...
3	where will Optimizing MySQL	what In this tutorial we will show ...
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('dbms');
id	a	b
SELECT * FROM t1 WHERE MATCH (b) AGAINST ('show');
id	a	b
3	where will Optimizing MySQL	what In this tutorial we will show ...
SELECT * FROM t1 WHERE MATCH (b) AGAINST ('dbms') AND MATCH (a,b) AGAINST ('transactions');
id	a	b
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('transactions');
id	a	b
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('"dbms stands"@05' IN BOOLEAN MODE);
id	a	b
1	MySQL from Tutorial	DBMS stands for DataBase ...
SELECT * FROM t1 WHERE MATCH (a,b) AGAINST ('NULL' IN BOOLEAN MODE);
id	a	b
SELECT * FROM t1 WHERE b IS NULL or a IS NULL;
id	a	b
9	NULL	NULL
CREATE TABLE t1_join AS SELECT * FROM t1;
CREATE FULLTEXT INDEX idx on t1_join (a,b) WITH PARSER simple_parser;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
SELECT t1.id,t1_join.id FROM t1,t1_join WHERE MATCH (t1.a,t1.b) AGAINST ('Tutorial') AND t1.id = t1_join.id ;
id	id
1	1
3	3
SELECT * FROM t1 WHERE MATCH (a) AGAINST ('NULL');
ERROR HY000: Can't find FULLTEXT index matching the column list
DROP TABLE t1;
DROP TABLE t1_join;
CREATE TABLE t1 (
a VARCHAR(200),
FULLTEXT (a) WITH PARSER ngram
)ENGINE = InnoDB;
SELECT * FROM t1 WHERE MATCH(a) AGAINST("')'@7" IN BOOLEAN MODE);
a
DROP TABLE t1;
UNINSTALL PLUGIN simple_parser;
# restart
