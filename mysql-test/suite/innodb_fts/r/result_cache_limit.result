CREATE TABLE t1 (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
a VARCHAR(200),
b TEXT
) ENGINE= InnoDB;
CREATE FULLTEXT INDEX idx on t1 (a,b);
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
INSERT INTO t1 (a,b) VALUES
('MySQL from Tutorial','DBMS stands for DataBase ...')  ,
('when To Use MySQL Well','After that you went through a ...'),
('where will Optimizing MySQL','what In this tutorial we will show ...'),
('MySQL from Tutorial','DBMS stands for DataBase ...')  ,
('when To Use MySQL Well','After that you went through a ...'),
('where will Optimizing MySQL','what In this tutorial we will show ...'),
('MySQL from Tutorial','DBMS stands for DataBase ...')  ,
('when To Use MySQL Well','After that you went through a ...'),
('where will Optimizing MySQL','what In this tutorial we will show ...');
SET SESSION debug="+d,fts_instrument_result_cache_limit";
SELECT COUNT(*) FROM t1 WHERE MATCH (a,b) AGAINST ('mysql' IN BOOLEAN MODE);
COUNT(*)
9
SELECT COUNT(*) FROM t1 WHERE MATCH (a,b) AGAINST ('mysql' WITH QUERY EXPANSION);
ERROR HY000: FTS query exceeds result cache limit
SELECT COUNT(*) FROM t1 WHERE MATCH (a,b) AGAINST ('"mysql database"' IN BOOLEAN MODE);
ERROR HY000: FTS query exceeds result cache limit
SELECT COUNT(*) FROM t1 WHERE MATCH (a,b) AGAINST ('"mysql database" @ 5' IN BOOLEAN MODE);
ERROR HY000: FTS query exceeds result cache limit
SET SESSION debug="-d,fts_instrument_result_cache_limit";
DROP TABLE t1;
SET GLOBAL innodb_ft_result_cache_limit=default;
