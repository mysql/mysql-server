'#--------------------FN_DYNVARS_033_01-------------------------#'
SET @@global.ft_boolean_syntax = ' -+()<>~*:``&|';
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@global.ft_boolean_syntax;
@@global.ft_boolean_syntax
 -+()<>~*:``&|
SET @@global.ft_boolean_syntax = '+ -><()~*:""&|';
'connect (con2,localhost,root,,,,)'
'connection con2'
SELECT @@global.ft_boolean_syntax;
@@global.ft_boolean_syntax
+ -><()~*:""&|
'#--------------------FN_DYNVARS_033_02-------------------------#'
'connection default'
DROP TABLE IF EXISTS t1;
CREATE TABLE articles (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200),
body TEXT,
FULLTEXT (title,body)
);
INSERT INTO articles (title,body) VALUES
('MySQL Tutorial','DBMS stands for DataBase ...'),
('How To',''),
('How To Use MySQL Well','After you went through a ...'),
('Optimizing MySQL','In this tutorial we will show .... Run command line ...'),
('1001 MySQL Tricks','1. Never run mysqld as root. 2. ...'),
('100 Tips for Myisam','1. Myisam is faster than innodb 2. Tricks and Tips for Myisam...'),
('MySQL vs. YourSQL','In the following database comparison ...'),
('MySQL Security','When configured properly, MySQL ...'),
('Database Security','Configuring MySQL for ...');
SET @@global.ft_boolean_syntax = DEFAULT;
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('+mySQL -yourSQL' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show .... Run command line ...
5	1001 MySQL Tricks	1. Never run mysqld as root. 2. ...
8	MySQL Security	When configured properly, MySQL ...
9	Database Security	Configuring MySQL for ...
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('+MySQL +YourSQL' IN BOOLEAN MODE);
id	title	body
7	MySQL vs. YourSQL	In the following database comparison ...
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('MySQL' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show .... Run command line ...
5	1001 MySQL Tricks	1. Never run mysqld as root. 2. ...
7	MySQL vs. YourSQL	In the following database comparison ...
8	MySQL Security	When configured properly, MySQL ...
9	Database Security	Configuring MySQL for ...
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('mysql tutorial dbms' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show .... Run command line ...
5	1001 MySQL Tricks	1. Never run mysqld as root. 2. ...
7	MySQL vs. YourSQL	In the following database comparison ...
8	MySQL Security	When configured properly, MySQL ...
9	Database Security	Configuring MySQL for ...
SELECT id,title,body, (MATCH (title,body) 
AGAINST ('+security configuring' IN BOOLEAN MODE)) AS relevance
FROM articles WHERE MATCH (title,body) 
AGAINST ('+security configuring' IN BOOLEAN MODE);
id	title	body	relevance
8	MySQL Security	When configured properly, MySQL ...	1
9	Database Security	Configuring MySQL for ...	1.3333333730697632
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('"faster than"' IN BOOLEAN MODE);
id	title	body
6	100 Tips for Myisam	1. Myisam is faster than innodb 2. Tricks and Tips for Myisam...
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('+tutorial ~line' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for DataBase ...
4	Optimizing MySQL	In this tutorial we will show .... Run command line ...
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('10*' IN BOOLEAN MODE);
id	title	body
5	1001 MySQL Tricks	1. Never run mysqld as root. 2. ...
SELECT id,title,body, (MATCH (title,body) 
AGAINST ('+MySQL +(>show <dbms)' IN BOOLEAN MODE)) AS relevance
FROM articles WHERE MATCH (title,body) 
AGAINST ('+MySQL +(>show <dbms)' IN BOOLEAN MODE)
ORDER BY relevance DESC;
id	title	body	relevance
4	Optimizing MySQL	In this tutorial we will show .... Run command line ...	1.25
1	MySQL Tutorial	DBMS stands for DataBase ...	0.8333333730697632
'---try setting different operators. Default '+ -><()~*:""&|'--'
SET @@global.ft_boolean_syntax='~ /!@#$%^&*()-';
SELECT * FROM articles WHERE MATCH (title,body)
AGAINST ('~mySQL /yourSQL' IN BOOLEAN MODE);
id	title	body
1	MySQL Tutorial	DBMS stands for DataBase ...
3	How To Use MySQL Well	After you went through a ...
4	Optimizing MySQL	In this tutorial we will show .... Run command line ...
5	1001 MySQL Tricks	1. Never run mysqld as root. 2. ...
8	MySQL Security	When configured properly, MySQL ...
9	Database Security	Configuring MySQL for ...
SET @@global.ft_boolean_syntax=DEFAULT;
DROP TABLE articles;
