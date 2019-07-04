CREATE TABLE f(f FLOAT);
CREATE TABLE f_fix(f FLOAT(5,2));
CREATE TABLE d(d DOUBLE);
CREATE TABLE d_fix(d DOUBLE(5,2));

INSERT INTO f VALUES (-3.4028234663852886e+38), (0), (3.4028234663852886e+38);
INSERT INTO f_fix VALUES (-999.00), (0), (999.99);
INSERT INTO d VALUES (-1.7976931348623157E+308), (0), (1.7976931348623157E+308);
INSERT INTO d_fix VALUES (-999.00), (0), (999.99);
ANALYZE TABLE f;
ANALYZE TABLE f_fix;
ANALYZE TABLE d;
ANALYZE TABLE d_fix;

# 65 decimal digits max in MySQL decimal type
let $max_decimal=99999999999999999999999999999999999999999999999999999999999999999.0;
--echo # With float we should see folding
let $query=SELECT * FROM f WHERE f < $max_decimal;
eval $query;
eval EXPLAIN $query;

--echo # With double, only type adjustment
let $query=SELECT * FROM d WHERE d < $max_decimal;
eval $query;
eval EXPLAIN $query;

let $c_1e38=100000000000000000000000000000000000000.0;
let $c_1e39=1000000000000000000000000000000000000000.0;
--echo # Check around max float in decimal
let $query=SELECT * FROM f WHERE f < $c_1e38;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM f WHERE f > $c_1e38;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM f WHERE f < $c_1e39;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM f WHERE f > $c_1e39;
eval $query;
eval EXPLAIN $query;

--echo # Then compare with a DOUBLE field
let $query=SELECT * FROM d WHERE d < $c_1e39;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM d WHERE d > $c_1e39;
eval $query;
eval EXPLAIN $query;

--echo Then with the two fixed real type fields, both of which
--echo should be folded
let $query=SELECT * FROM f_fix WHERE f < $c_1e39;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM f_fix WHERE f > $c_1e39;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM d_fix WHERE d < $c_1e39;
eval $query;
eval EXPLAIN $query;

let $query=SELECT * FROM d_fix WHERE d > $c_1e39;
eval $query;
eval EXPLAIN $query;


DROP TABLE f, f_fix, d, d_fix;
