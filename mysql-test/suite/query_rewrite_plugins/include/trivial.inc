DELETE FROM query_rewrite.rewrite_rules;

INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SELECT * FROM mydb.mytable WHERE mycol = ?', 'select "rule1"' );

CALL query_rewrite.flush_rewrite_rules();

CREATE DATABASE mydb;
CREATE TABLE mydb.mytable ( mycol char(3), mycol2 CHAR(100) );
INSERT INTO mydb.mytable VALUES ( 'xxx', 'Not supposed to be seeing this.' );

SELECT * FROM mydb.mytable WHERE mycol = 'xxx';

DROP DATABASE mydb;
