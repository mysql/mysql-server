#
# Bug#30350111: UTF8MB4_0900_BIN STRINGS CANNOT BE COMPARED WITH
#               UTF8MB4_0900_AI_CI STRINGS
#
CREATE TABLE t(id INT PRIMARY KEY AUTO_INCREMENT,
col_aici VARCHAR(10) COLLATE utf8mb4_0900_ai_ci,
col_bin VARCHAR(10) COLLATE utf8mb4_bin,
col_0900_bin VARCHAR(10) COLLATE utf8mb4_0900_bin);
INSERT INTO t(col_aici)
VALUES ('abc'), ('ABC'), ('Abc'), ('abc '), ('ABC  '), ('Abc   ');
UPDATE t SET col_bin = col_aici, col_0900_bin = col_aici;
SELECT CONCAT('''', t1.col_aici, ''' = ''', t2.col_aici, '''')
FROM t t1, t t2
WHERE t1.col_aici = t2.col_bin
ORDER BY t1.id, t2.id;
CONCAT('''', t1.col_aici, ''' = ''', t2.col_aici, '''')
'abc' = 'abc'
'abc' = 'abc '
'ABC' = 'ABC'
'ABC' = 'ABC  '
'Abc' = 'Abc'
'Abc' = 'Abc   '
'abc ' = 'abc'
'abc ' = 'abc '
'ABC  ' = 'ABC'
'ABC  ' = 'ABC  '
'Abc   ' = 'Abc'
'Abc   ' = 'Abc   '
SELECT CONCAT('''', t1.col_aici, ''' = ''', t2.col_aici, '''')
FROM t t1, t t2
WHERE t1.col_aici = t2.col_0900_bin
ORDER BY t1.id, t2.id;
CONCAT('''', t1.col_aici, ''' = ''', t2.col_aici, '''')
'abc' = 'abc'
'ABC' = 'ABC'
'Abc' = 'Abc'
'abc ' = 'abc '
'ABC  ' = 'ABC  '
'Abc   ' = 'Abc   '
SELECT CONCAT('''', t1.col_aici, ''' = ''', t2.col_aici, '''')
FROM t t1, t t2
WHERE t1.col_bin = t2.col_0900_bin
ORDER BY t1.id, t2.id;
ERROR HY000: Illegal mix of collations (utf8mb4_bin,IMPLICIT) and (utf8mb4_0900_bin,IMPLICIT) for operation '='
SELECT CONCAT('''', t1.col_aici, ''' = ''', t2.col_aici, '''')
FROM t t1, t t2
WHERE t1.col_0900_bin = t2.col_bin
ORDER BY t1.id, t2.id;
ERROR HY000: Illegal mix of collations (utf8mb4_0900_bin,IMPLICIT) and (utf8mb4_bin,IMPLICIT) for operation '='
SELECT col_aici FROM t UNION SELECT col_bin FROM t;
col_aici
ABC
Abc
abc
SELECT col_aici FROM t UNION SELECT col_0900_bin FROM t;
col_aici
ABC
ABC  
Abc
Abc   
abc
abc 
SELECT col_bin FROM t UNION SELECT col_0900_bin FROM t;
ERROR HY000: Illegal mix of collations for operation 'UNION'
SELECT col_0900_bin FROM t UNION SELECT col_bin FROM t;
ERROR HY000: Illegal mix of collations for operation 'UNION'
SELECT col_bin FROM t UNION SELECT col_0900_bin FROM t
UNION SELECT col_aici FROM t;
ERROR HY000: Illegal mix of collations for operation 'UNION'
SELECT col_bin FROM t UNION SELECT col_0900_bin FROM t
UNION SELECT col_aici COLLATE utf8mb4_0900_ai_ci FROM t;
col_bin
ABC  
Abc   
abc
abc 
SELECT col_bin FROM t UNION SELECT col_0900_bin FROM t
UNION SELECT col_aici COLLATE utf8mb4_bin FROM t;
col_bin
ABC
Abc
abc
SELECT col_bin FROM t UNION SELECT col_0900_bin FROM t
UNION SELECT col_aici COLLATE utf8mb4_0900_bin FROM t;
col_bin
ABC
ABC  
Abc
Abc   
abc
abc 
DROP TABLE t;
CREATE TABLE t(x VARCHAR(10) BINARY);
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
SELECT COLLATION(MAX(x)) FROM t;
COLLATION(MAX(x))
utf8mb4_bin
DROP TABLE t;
