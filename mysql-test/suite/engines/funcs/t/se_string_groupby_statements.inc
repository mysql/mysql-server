--sorted_result
SELECT c1 FROM t1 GROUP BY c1;
--sorted_result
SELECT c1,COUNT(*) FROM t1 GROUP BY c1;
--sorted_result
SELECT c1,COUNT(*),MAX(c3) FROM t1 WHERE c1=@value GROUP BY c1;
--sorted_result
SELECT c1,COUNT(*),MAX(c3) FROM t1 WHERE c1=@value OR c2 = '#C2' GROUP BY c1;
--sorted_result
SELECT c1,c2 FROM t1 GROUP BY c1,c2;
--sorted_result
SELECT c1,c2,COUNT(*) FROM t1 GROUP BY c1,c2;
--sorted_result
SELECT c1,c2,COUNT(*),MAX(c3) FROM t1 WHERE c1=@value GROUP BY c1,c2;
--sorted_result
SELECT c1,c2,COUNT(*),MAX(c3) FROM t1 WHERE c1=@value OR c2 = '#C2' GROUP BY c1,c2;
--sorted_result
SELECT c2,c1 FROM t1 GROUP BY c2,c1;
--sorted_result
SELECT c2,c1,COUNT(*) FROM t1 GROUP BY c2,c1;
--sorted_result
SELECT c2,c1,COUNT(*),MAX(c3) FROM t1 WHERE c1=@value GROUP BY c2,c1;
--sorted_result
SELECT c2,c1,COUNT(*),MAX(c3) FROM t1 WHERE c1=@value OR c2 = '#C2' GROUP BY c2,c1;
