# Testing GROUP MIN/MAX with multiple infix ranges.
let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  =  WHERE (b = 2 OR b = 15 OR b = 3) AND (c = 3 OR c = 40) GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (b = 2 OR b = 15) AND (c = 3 OR c = 40) GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (b = 2 OR b = 15) AND (c = 3 OR c IS NULL) GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (b IS NULL OR b = 15) AND (c = 3 OR c = 40) GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (b IS NULL OR b = 2) AND (c = 3 OR c = 40) GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a FROM t;
let $cond  = WHERE (b = 2 OR b = 15 OR b = 3) AND (c = 3 OR c = 40) GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (b = 2 OR b = 15) AND (c = 3 OR c = 40) GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (b = 2 OR b = 15 OR b = 3) AND (c = 3 OR c = 40)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT DISTINCT a FROM t;
let $cond  = WHERE a = 1 AND (b = 2 OR b = 15);
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  = WHERE a = 1 AND (c = 3 OR c = 40) AND (d = 1 OR d = 9)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

# Testing GROUP MIN/MAX with multiple prefix ranges and infix ranges.

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (a = 1 OR a = 12) AND (b = 2 OR b = 15) AND (c = 3 OR c = 40)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (a < 2 OR a > 11) AND (b = 2 OR b = 15) AND (c = 3 OR c = 40)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT DISTINCT a, b FROM t;
let $cond  = WHERE (a = 1 OR a = 12) AND (b = 2 OR b = 15) AND (c = 3 OR c = 40);
--source include/group_skip_scan_ext_query.inc

let $query = SELECT DISTINCT a FROM t;
let $cond  = WHERE (a = 1 OR a = 12) AND (b = 2 OR b = 15);
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (a = 1 OR a = 12) AND (b < 4 OR b > 10) AND (c = 3 OR c = 40)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (a = 1 OR a > 9) AND (b = 2 OR b = 15) AND (c = 3 OR c = 40)
             GROUP BY a;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT COUNT(DISTINCT a, b) FROM t;
let $cond  = WHERE (a = 1 OR a = 12) AND (b = 2 OR b = 15) AND (c = 3 OR c = 40);
--source include/group_skip_scan_ext_query.inc

let $query = SELECT SUM(DISTINCT a), AVG(DISTINCT a) FROM t;
let $cond  = WHERE (b = 2 OR b = 15) AND (c = 3 OR c = 40);
--source include/group_skip_scan_ext_query.inc

let $query = SELECT SUM(DISTINCT a), AVG(DISTINCT a), COUNT(DISTINCT a) FROM t;
let $cond  = WHERE (b = 2 OR b = 15) AND (c = 3 OR c = 40);
--source include/group_skip_scan_ext_query.inc

# No group-by loose index scan due to cost.
let $query = SELECT a, MAX(d), MIN(d) FROM t;
let $cond = WHERE (a > 11) AND (b = 2 OR b = 15) AND
            (c = 3 OR c = 1 OR c = 2 OR c = 4)
            GROUP BY a;
--source include/group_skip_scan_ext_query.inc

# Testing GROUP MIN/MAX with multiple prefix ranges, infix ranges and
# min_max ranges.

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (c = 3 OR c = 40) AND (d  > 7 OR d =2) GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (c = 3 OR c = 40) AND (d  > 7 OR d =2 OR d IS NULL)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MIN(d) FROM t;
let $cond  = WHERE (a > 5) AND (c = 3 OR c = 40) AND
             (d  < -24 OR d = 3 OR d IS NULL)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (a > 5) AND (c = 3 OR c = 40) AND (d  <  1 OR d = 9)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MAX(d), MIN(d) FROM t;
let $cond  = WHERE (a > 9) AND (c = 3 OR c = 40) AND
             (d  =  1 OR d = 9 OR d IS NULL)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MIN(d), MAX(d) FROM t;
let $cond  = WHERE (a > 9) AND (c = 3 OR c = 40) AND
             (d  =  0 OR d = 9 OR d IS NULL)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MIN(d), MAX(d) FROM t;
let $cond  = WHERE (a > 9) AND (c = 3 OR c = 40) AND
             (d  > 0 AND d < 2 OR d > 3 AND d < 5 OR d IS NULL)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b, MIN(d) FROM t;
let $cond  = WHERE (a > 9) AND (c = 3 OR c = 40) AND
             (d < 2 OR d > 5 OR d IS NULL)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b , max(d) FROM t;
let $cond  = WHERE (a >= 10) AND (c = 3 or c=40) AND
             (d is NULL or d = -10 or d = -23)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

let $query = SELECT a, b , min(d) FROM t;
let $cond  = WHERE (a >= 10) AND (c = 3 or c=40) AND
             (d is NULL or d = -10 or d = -23)
             GROUP BY a, b;
--source include/group_skip_scan_ext_query.inc

# No prefix and min/max ranges
let $query = SELECT a, MIN(b), MAX(b) FROM t;
let $cond  = WHERE a > 9 GROUP BY a;
--source include/group_skip_scan_ext_query.inc
