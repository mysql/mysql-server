INSTALL PLUGIN simple_parser SONAME 'mypluglib.so';
# Test Part 2: Create Index Test(CREATE TABLE WITH FULLTEXT INDEX)
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body) WITH PARSER simple_parser
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...'),
('How To Use MySQL Well','After you went through a ...'),
('Optimizing MySQL','In this tutorial we will show ...'),
('1001 MySQL Tricks','How to use full-text search engine'),
('Go MySQL Tricks','How to use full text search engine');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	How To Use MySQL Well	After you went through a ...
3	Optimizing MySQL	In this tutorial we will show ...
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('will go');
id	title	body
# Test plugin parser tokenizer difference
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full-text');
id	title	body
4	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full text');
id	title	body
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body
DROP TABLE articles;
# Test Part 3: Row Merge Create Index Test(ALTER TABLE ADD FULLTEXT INDEX)
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...'),
('How To Use MySQL Well','After you went through a ...'),
('Optimizing MySQL','In this tutorial we will show ...'),
('1001 MySQL Tricks','How to use full-text search engine'),
('Go MySQL Tricks','How to use full text search engine');
ALTER TABLE articles ADD FULLTEXT INDEX (title, body) WITH PARSER simple_parser;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	How To Use MySQL Well	After you went through a ...
3	Optimizing MySQL	In this tutorial we will show ...
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('will go');
id	title	body
# Test plugin parser tokenizer difference
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full-text');
id	title	body
4	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full text');
id	title	body
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full-text' WITH QUERY EXPANSION);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	How To Use MySQL Well	After you went through a ...
3	Optimizing MySQL	In this tutorial we will show ...
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full text' WITH QUERY EXPANSION);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	How To Use MySQL Well	After you went through a ...
3	Optimizing MySQL	In this tutorial we will show ...
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body
DROP TABLE articles;
# Test Part 3 END
SET SESSION debug="+d,fts_instrument_use_default_parser";
# Test Part 4: Create Index Test with Default/Internal Parser
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body) WITH PARSER simple_parser
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...'),
('How To Use MySQL Well','After you went through a ...'),
('Optimizing MySQL','In this tutorial we will show ...'),
('1001 MySQL Tricks','How to use full-text search engine'),
('Go MySQL Tricks','How to use full text search engine');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	How To Use MySQL Well	After you went through a ...
3	Optimizing MySQL	In this tutorial we will show ...
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('will go');
id	title	body
# Test plugin parser tokenizer difference
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full-text');
id	title	body
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full text');
id	title	body
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
# Test Part 5: Row Merge Create Index Test with Default/Internal Parser
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...'),
('How To Use MySQL Well','After you went through a ...'),
('Optimizing MySQL','In this tutorial we will show ...'),
('1001 MySQL Tricks','How to use full-text search engine'),
('Go MySQL Tricks','How to use full text search engine');
ALTER TABLE articles ADD FULLTEXT INDEX (title, body) WITH PARSER simple_parser;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('mysql');
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	How To Use MySQL Well	After you went through a ...
3	Optimizing MySQL	In this tutorial we will show ...
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('will go');
id	title	body
# Test plugin parser tokenizer difference
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full-text');
id	title	body
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('full text');
id	title	body
4	1001 MySQL Tricks	How to use full-text search engine
5	Go MySQL Tricks	How to use full text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
DROP TABLE articles;
# Test Part 6: Test Query Parser with Default/Internal Parser
SET GLOBAL innodb_ft_enable_diag_print = 1;
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body) WITH PARSER simple_parser
) ENGINE=InnoDB;
INSERT INTO articles (title, body) VALUES
('MySQL Tutorial','DBMS stands for MySQL DataBase ...'),
('MySQL Tutorial','DBMS stands for MySQL good one DataBase ...'),
('How To Use MySQL Well','After you went through a ...'),
('Optimizing MySQL','In this tutorial we will show ...'),
('1001 MySQL Tricks','How to use full-text search engine');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+mysql -database' IN BOOLEAN MODE);
id	title	body
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show ...
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('>mysql <database ~search' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	MySQL Tutorial	DBMS stands for MySQL good one DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show ...
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+(mysql database) -engine' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	MySQL Tutorial	DBMS stands for MySQL good one DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show ...
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+(mysql database -engine' IN BOOLEAN MODE);
id	title	body
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql"' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	MySQL Tutorial	DBMS stands for MySQL good one DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show ...
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"for mysql"' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
2	MySQL Tutorial	DBMS stands for MySQL good one DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show ...
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"mysql database"' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"full text search"' IN BOOLEAN MODE);
id	title	body
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"full text search engine"' IN BOOLEAN MODE);
id	title	body
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+"dbms stands" -good' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+("dbms stands") -good' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+("dbms stands" search) -good' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+("dbms stands" "full text") -good' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for MySQL DataBase ...
5	1001 MySQL Tricks	How to use full-text search engine
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('("msyql database")@3' IN BOOLEAN MODE);
id	title	body
DROP TABLE articles;
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
# Test Part 7: Test Different Charset
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body) WITH PARSER simple_parser
) ENGINE=InnoDB DEFAULT CHARACTER SET gb2312 COLLATE gb2312_chinese_ci;
INSERT INTO articles (title, body) VALUES
('数据库 是 数据 的 结构化 集合','它 可以 是 任何 东西'),
('从 简单 的 购物清单 到 画展','或 企业网络 中 的 海量信息'),
('要 想 将 数据 添加 到 数据库','或 访问、处理 计算机 数据库 中 保存 的 数据'),
('需要 使用 数据库 管理系统','计算机 是 处理 大量 数据 的 理想 工具');
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('数据库');
id	title	body
1	数据库 是 数据 的 结构化 集合	它 可以 是 任何 东西
3	要 想 将 数据 添加 到 数据库	或 访问、处理 计算机 数据库 中 保存 的 数据
4	需要 使用 数据库 管理系统	计算机 是 处理 大量 数据 的 理想 工具
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('数据');
id	title	body
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('数据*');
id	title	body
1	数据库 是 数据 的 结构化 集合	它 可以 是 任何 东西
3	要 想 将 数据 添加 到 数据库	或 访问、处理 计算机 数据库 中 保存 的 数据
4	需要 使用 数据库 管理系统	计算机 是 处理 大量 数据 的 理想 工具
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('+数据库 -计算机');
id	title	body
1	数据库 是 数据 的 结构化 集合	它 可以 是 任何 东西
SELECT * FROM articles WHERE
MATCH(title, body) AGAINST('"计算机 数据库"' IN BOOLEAN MODE);
id	title	body
3	要 想 将 数据 添加 到 数据库	或 访问、处理 计算机 数据库 中 保存 的 数据
DROP TABLE articles;
SET GLOBAL innodb_ft_enable_diag_print = 0;
SET SESSION debug="-d,fts_instrument_use_default_parser";
# Test Part 9:# Abort the operation in dict_create_index_step
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title, body) WITH PARSER simple_parser
) ENGINE=InnoDB;
SET SESSION debug="+d,ib_dict_create_index_tree_fail";
CREATE FULLTEXT INDEX idx ON articles(body);
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
SET SESSION debug="-d,ib_dict_create_index_tree_fail";
DROP TABLE articles;
UNINSTALL PLUGIN simple_parser;
