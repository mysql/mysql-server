# These same selects are used many times in innodb_isolation.test 

--echo ########## innodb_isolation_selects.inc ##########
SELECT * FROM t1;
SELECT COUNT(*) FROM t1;         # Uses handler::records() and clustered index
SELECT COUNT(*) FROM t3;         # Uses handler::records() and clustered index
SELECT COUNT(c1) FROM t1;        # Uses handler::records() and clustered index
SELECT COUNT(c2) FROM t1;        # Uses secondary index k2
SELECT COUNT(c3) FROM t1;        # Uses clustered index
SELECT SUM(c1) FROM t1;          # Uses secondary index
SELECT SUM(c2) FROM t1;          # Uses secondary index
SELECT AVG(c1), MAX(c1), MIN(c2), AVG(c3), SUM(c4) FROM t1; # Uses clustered index
SELECT AVG(c1), MAX(c1), MIN(c2), AVG(c3), SUM(c4) FROM t3; # Uses clustered index
SELECT c1, c2 FROM t1 WHERE c1 > ((SELECT COUNT(*) FROM t1) / 2);
SELECT c1, c2 FROM t1 WHERE c1 > ((SELECT COUNT(*) FROM t3) / 2);
SELECT COUNT(c2) FROM t1 WHERE c1 > ((SELECT COUNT(*) FROM t1) / 2);
SELECT COUNT(c2) FROM t1 WHERE c1 > ((SELECT COUNT(*) FROM t3) / 2);
SELECT COUNT(*) FROM t1 WHERE c1 > (SELECT AVG(c1) FROM t1);
SELECT COUNT(*) FROM t1 WHERE c1 > (SELECT AVG(c1) FROM t3);
--echo ###############################################

