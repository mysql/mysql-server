DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (
id INT UNSIGNED,
a JSON DEFAULT NULL,
PRIMARY KEY (id),
KEY ((CAST(JSON_EXTRACT(a,_utf8mb4'$.b') AS CHAR(255) ARRAY)))
) ENGINE=INNODB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ;
CREATE PROCEDURE fill()
BEGIN
DECLARE v_i BIGINT UNSIGNED DEFAULT 0;
REPEAT
SET v_i:=v_i+1;
REPLACE INTO t1(id,a) VALUES(FLOOR(RAND()*100000),CONCAT('{"a":"\'{ab:a,ba:',FLOOR(RAND()*100000),',bab:baa}\'","b":"\'{a:bbb,bab:true,bb:[',FLOOR(RAND()*100000),',8,7,1,9,10,8,7,3]}\'","ba":"b","baa":[6,1,1,2,9,8,0,5,2,4]}'));
UNTIL v_i=200 END REPEAT;
END |
CREATE PROCEDURE upd()
BEGIN
DECLARE v_i BIGINT UNSIGNED DEFAULT 0;
REPEAT
SET v_i:=v_i+1;
UPDATE t1 SET a= CONCAT('{"b":',FLOOR(RAND()*100000),',"aba":"\'{a:[a,aa,b,aab,aa,bba,baa,a],b:bba}\'","b":',FLOOR(RAND()*100000),',"bb":2}');
UNTIL v_i = 10 END REPEAT;
END |
REPLACE INTO t1(id,a) VALUES(7,CONCAT('{"a":"\'{ab:a,ba:',FLOOR(RAND()*100000),',bab:baa}\'","b":"\'{a:bbb,bab:true,bb:[',FLOOR(RAND()*100000),',8,7,1,9,10,8,7,3]}\'","ba":"b","baa":[6,1,1,2,9,8,0,5,2,4]}'));
UPDATE t1 SET a= CONCAT('{"b":',FLOOR(RAND()*100000),',"aba":"\'{a:[a,aa,b,aab,aa,bba,baa,a],b:bba}\'","b":',FLOOR(RAND()*100000),',"bb":2}');
UPDATE t1 SET a= CONCAT('{"b":',FLOOR(RAND()*100000),',"aba":"\'{a:[a,aa,b,aab,aa,bba,baa,a],b:bba}\'","b":',FLOOR(RAND()*100000),',"bb":2}');
CALL fill();
CALL upd();
DROP PROCEDURE IF EXISTS fill;
DROP PROCEDURE IF EXISTS upd;
DROP TABLE t1;
