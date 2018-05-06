SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select row_number() over ()));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select row_number() over ()));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select ntile(3) over (order by null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select ntile(3) over (order by null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select avg(1) over ()));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select avg(1) over ()));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select sum(1) over ()));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select sum(1) over ()));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select count(1) over ()));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select count(1) over ()));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select rank() over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select rank() over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select dense_rank() over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select dense_rank() over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a - 1 IN ( (select percent_rank() over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select percent_rank() over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select cume_dist() over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select cume_dist() over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select lead(1, 0) over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select lead(1, 0) over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select nth_value(1, 1) over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a + 1 IN ( (select nth_value(1, 1) over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select first_value(1) over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a +1  IN ( (select first_value(1) over (ORDER BY null)));

SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a     IN ( (select last_value(1) over (ORDER BY null)));
SELECT SUM( distinct a ) FROM t1 GROUP BY a HAVING a +1  IN ( (select last_value(1) over (ORDER BY null)));
