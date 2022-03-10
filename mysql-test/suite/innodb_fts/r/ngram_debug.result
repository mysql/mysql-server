SET GLOBAL innodb_ft_enable_diag_print = 1;
# Test Case 1: Test English with Default(latin1) Charset
SET DEBUG="+d,fts_instrument_ignore_ngram_check";
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200)
) ENGINE=InnoDB;
ALTER TABLE articles ADD FULLTEXT INDEX (title) WITH PARSER ngram;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
INSERT INTO articles (title) VALUES
('what is M'),
('Myname is wonder'),
('My SQL is good'),
('MySQL database'),
('Mind a good thing'),
('Mind good idea');
SELECT * FROM articles WHERE
MATCH(title) AGAINST('m*' IN BOOLEAN MODE);
id	title
2	Myname is wonder
3	My SQL is good
4	MySQL database
5	Mind a good thing
6	Mind good idea
SELECT * FROM articles WHERE
MATCH(title) AGAINST('my*' IN BOOLEAN MODE);
id	title
2	Myname is wonder
3	My SQL is good
4	MySQL database
SELECT * FROM articles WHERE
MATCH(title) AGAINST('+mind -mysql' IN BOOLEAN MODE);
id	title
5	Mind a good thing
6	Mind good idea
SELECT * FROM articles WHERE
MATCH(title) AGAINST('+(mind thing) -idea' IN BOOLEAN MODE);
id	title
5	Mind a good thing
SELECT * FROM articles WHERE
MATCH(title) AGAINST('mysql');
id	title
2	Myname is wonder
3	My SQL is good
4	MySQL database
SELECT * FROM articles WHERE
MATCH(title) AGAINST('mysql' IN BOOLEAN MODE);
id	title
4	MySQL database
SELECT * FROM articles WHERE
MATCH(title) AGAINST('"mysql"' IN BOOLEAN MODE);
id	title
4	MySQL database
SELECT * FROM articles WHERE
MATCH(title) AGAINST('"my sql"' IN BOOLEAN MODE);
id	title
3	My SQL is good
SET DEBUG="-d,fts_instrument_ignore_ngram_check";
DROP TABLE articles;
SET NAMES utf8mb4;
# Test Case 2: Test Stopwords
SET DEBUG="+d,fts_instrument_ignore_ngram_check";
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT
) ENGINE=InnoDB DEFAULT CHARACTER SET gb2312 COLLATE gb2312_chinese_ci;
CREATE FULLTEXT INDEX ft_index ON articles(title, body) WITH PARSER ngram;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
INSERT INTO articles (title, body) VALUES
('从简单购物清单到画展','或企业网络中的海量数据'),
('要想将数据添加到数据库','访问、处理计算机数据库中保存的数据'),
('从简单的购物清单到画展','或企业网络中的海量数据'),
('要想将数据添加到数据库','或访问、处理计算机数据库中保存的数据'),
('数据库是数据的结构化集合','它可以是任何东西');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('的* 或* 东西' IN BOOLEAN MODE);
id	title	body
1	从简单购物清单到画展	或企业网络中的海量数据
2	要想将数据添加到数据库	访问、处理计算机数据库中保存的数据
3	从简单的购物清单到画展	或企业网络中的海量数据
4	要想将数据添加到数据库	或访问、处理计算机数据库中保存的数据
5	数据库是数据的结构化集合	它可以是任何东西
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('简单的' IN BOOLEAN MODE);
id	title	body
3	从简单的购物清单到画展	或企业网络中的海量数据
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('或访问' IN BOOLEAN MODE);
id	title	body
4	要想将数据添加到数据库	或访问、处理计算机数据库中保存的数据
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"数据的结构化"' IN BOOLEAN MODE);
id	title	body
5	数据库是数据的结构化集合	它可以是任何东西
SET DEBUG="-d,fts_instrument_ignore_ngram_check";
DROP TABLE articles;
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT
) ENGINE=InnoDB DEFAULT CHARACTER SET gb2312 COLLATE gb2312_chinese_ci;
CREATE TABLE user_stopword(value varchar(30)) ENGINE = InnoDB
DEFAULT CHARACTER SET gb2312 COLLATE gb2312_chinese_ci;
INSERT INTO user_stopword VALUES('的'),('或'),('东西'),('ab');
SET GLOBAL innodb_ft_server_stopword_table = 'test/user_stopword';
INSERT INTO articles (title, body) VALUES
('从简单购物清单到画展','或企业网络中的海量数据'),
('要想将数据添加到数据库','访问、处理计算机数据库中保存的数据');
CREATE FULLTEXT INDEX ft_index ON articles(title, body) WITH PARSER ngram;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
INSERT INTO articles (title, body) VALUES
('从简单的购物清单到画展','或企业网络中的海量数据'),
('要想将数据添加到数据库','或访问、处理计算机数据库中保存的数据'),
('要想将数据添加到数据库','或访问、处理计算机数据库中保存的数据 abc xyz'),
('数据库是数据的结构化集合','它可以是任何东西');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('的* 或* 东西' IN BOOLEAN MODE);
id	title	body
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('简单的' IN BOOLEAN MODE);
id	title	body
3	从简单的购物清单到画展	或企业网络中的海量数据
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('或访问' IN BOOLEAN MODE);
id	title	body
2	要想将数据添加到数据库	访问、处理计算机数据库中保存的数据
4	要想将数据添加到数据库	或访问、处理计算机数据库中保存的数据
5	要想将数据添加到数据库	或访问、处理计算机数据库中保存的数据 abc xyz
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"数据的结构化"' IN BOOLEAN MODE);
id	title	body
6	数据库是数据的结构化集合	它可以是任何东西
SELECT * FROM user_stopword WHERE value like 'ab' ;
value
ab
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"ab"' );
id	title	body
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('数据 +ab' IN BOOLEAN MODE);
id	title	body
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('数据 (+xyz -bc)' IN BOOLEAN MODE);
id	title	body
1	从简单购物清单到画展	或企业网络中的海量数据
2	要想将数据添加到数据库	访问、处理计算机数据库中保存的数据
3	从简单的购物清单到画展	或企业网络中的海量数据
4	要想将数据添加到数据库	或访问、处理计算机数据库中保存的数据
5	要想将数据添加到数据库	或访问、处理计算机数据库中保存的数据 abc xyz
6	数据库是数据的结构化集合	它可以是任何东西
DROP TABLE user_stopword;
DROP TABLE articles;
SET GLOBAL innodb_ft_server_stopword_table=default;
# Test Case 3: Check with FTS debug points
CREATE TABLE articles (
seq INT AUTO_INCREMENT NOT NULL PRIMARY KEY,
c1 VARCHAR(200) CHARACTER SET big5 COLLATE big5_chinese_ci,
c2 CHAR(200) CHARACTER SET gbk COLLATE gbk_chinese_ci,
c3 CHAR(200) CHARACTER SET utf8,
j1 TEXT CHARACTER SET ujis COLLATE ujis_japanese_ci,
j2 VARCHAR(200) CHARACTER SET sjis COLLATE sjis_japanese_ci,
j3 VARCHAR(200) CHARACTER SET ujis,
k1 CHAR(200) CHARACTER SET euckr COLLATE euckr_korean_ci,
k2 CHAR(200) CHARACTER SET utf8,
e1 CHAR(200) CHARACTER SET latin1,
FULLTEXT KEY `con1` (c1),
FULLTEXT KEY `con2` (j1,j3),
FULLTEXT KEY `con3` (k2) WITH PARSER ngram
) ENGINE=InnoDB ;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SET DEBUG='+d,alter_table_rollback_new_index';
ALTER TABLE articles ADD FULLTEXT INDEX (c2(64));
ERROR HY000: Unknown error
ALTER TABLE articles ADD FULLTEXT INDEX (j1,j3);
ERROR HY000: Unknown error
ALTER TABLE articles ADD FULLTEXT INDEX (k1);
ERROR HY000: Unknown error
ALTER TABLE articles ADD FULLTEXT INDEX (e1);
ERROR HY000: Unknown error
SET DEBUG='-d,alter_table_rollback_new_index';
SET DEBUG="+d,ib_dict_create_index_tree_fail";
ALTER TABLE articles ADD FULLTEXT INDEX (c2(64));
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
ALTER TABLE articles ADD FULLTEXT INDEX (j1,j3);
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
ALTER TABLE articles ADD FULLTEXT INDEX (k1);
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
ALTER TABLE articles ADD FULLTEXT INDEX (e1);
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
SET DEBUG="-d,ib_dict_create_index_tree_fail";
ALTER TABLE articles ADD FULLTEXT INDEX (e1), ADD FULLTEXT INDEX (k1);
ERROR HY000: InnoDB presently supports one FULLTEXT index creation at a time
DROP TABLE articles;
SET GLOBAL innodb_ft_enable_diag_print = default;
