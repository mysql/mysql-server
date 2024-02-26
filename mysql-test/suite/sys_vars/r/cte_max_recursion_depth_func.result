#
# Bug#26136509 ADD A MAX_RECURSION VARIABLE TO LIMIT RECURSION IN CTES
#
SELECT @@cte_max_recursion_depth;
@@cte_max_recursion_depth
1000
SET @m = 1001;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION ALL
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 1001 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 1000;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION ALL
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
COUNT(*)
1000
SET cte_max_recursion_depth=10;
SET @m = 11;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION ALL
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 11 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 10;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION ALL
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
COUNT(*)
10
set cte_max_recursion_depth=default;
SET @m = 1001;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION ALL
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION ALL
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION ALL
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 1001 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 999;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION ALL
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION ALL
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION ALL
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
COUNT(*)
2995
SET cte_max_recursion_depth=10;
SET @m = 11;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION ALL
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION ALL
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION ALL
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 11 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 9;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION ALL
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION ALL
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION ALL
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
COUNT(*)
25
SET cte_max_recursion_depth=DEFAULT;
SET cte_max_recursion_depth=10;
SET @m = 11;
WITH RECURSIVE
onetwo(b) AS (SELECT 1 UNION SELECT 2),
qn AS (
SELECT 1 AS a, 0 AS b UNION ALL
SELECT 1+a, onetwo.b FROM onetwo, qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 11 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 10;
WITH RECURSIVE
onetwo(b) AS (SELECT 1 UNION SELECT 2),
qn AS (
SELECT 1 AS a, 0 AS b UNION ALL
SELECT 1+a, onetwo.b FROM onetwo, qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
COUNT(*)
1023
SET cte_max_recursion_depth=default;
SET @m = 1001;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION DISTINCT
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 1001 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 1000;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION DISTINCT
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
COUNT(*)
1000
SET cte_max_recursion_depth=10;
SET @m = 11;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION DISTINCT
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 11 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 10;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION DISTINCT
SELECT 1+a FROM qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
COUNT(*)
10
set cte_max_recursion_depth=default;
SET @m = 1001;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION DISTINCT
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION DISTINCT
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION DISTINCT
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 1001 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 999;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION DISTINCT
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION DISTINCT
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION DISTINCT
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
COUNT(*)
2995
SET cte_max_recursion_depth=10;
SET @m = 11;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION DISTINCT
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION DISTINCT
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION DISTINCT
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 11 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 9;
WITH RECURSIVE qn AS
(SELECT 1 AS a, 0 AS b UNION DISTINCT
SELECT 1+a, 1 FROM qn WHERE b IN (0,1) AND a<@m UNION DISTINCT
SELECT 1+a, 2 FROM qn WHERE b IN (0,2) AND a<@m UNION DISTINCT
SELECT 1+a, 3 FROM qn WHERE b IN (0,3) AND a<@m
) SELECT COUNT(*) FROM qn;
COUNT(*)
25
SET cte_max_recursion_depth=DEFAULT;
SET cte_max_recursion_depth=10;
SET @m = 11;
WITH RECURSIVE
onetwo(b) AS (SELECT 1 UNION SELECT 2),
qn AS (
SELECT 1 AS a, 0 AS b UNION DISTINCT
SELECT 1+a, onetwo.b FROM onetwo, qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 11 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
SET @m = 10;
WITH RECURSIVE
onetwo(b) AS (SELECT 1 UNION SELECT 2),
qn AS (
SELECT 1 AS a, 0 AS b UNION DISTINCT
SELECT 1+a, onetwo.b FROM onetwo, qn WHERE a<@m)
SELECT COUNT(*) FROM qn;
COUNT(*)
19
SET cte_max_recursion_depth=default;
SET cte_max_recursion_depth=0;
WITH RECURSIVE qn AS (
SELECT 1 AS a UNION ALL
SELECT 1+a FROM qn WHERE a<0)
SELECT COUNT(*) FROM qn;
ERROR HY000: Recursive query aborted after 1 iterations. Try increasing @@cte_max_recursion_depth to a larger value.
