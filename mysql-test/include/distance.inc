--echo #
--echo # Test coverage for vector DISTANCE() function.
--echo #

--echo #
--echo # 0) Prepare playground.
--echo #
CREATE TABLE t1 (id INT PRIMARY KEY, v1 VECTOR(1), v2 VECTOR(2));
INSERT INTO t1 VALUES (0, TO_VECTOR('[0]'), TO_VECTOR('[0, 0]')),
                      (1, TO_VECTOR('[1]'), TO_VECTOR('[1, 0]')),
                      (2, TO_VECTOR('[1]'), TO_VECTOR('[0, 1]')),
                      (3, TO_VECTOR('[2]'), TO_VECTOR('[1, 1]')),
                      (4, TO_VECTOR('[2]'), TO_VECTOR('[2, 0]')),
                      (98, TO_VECTOR('[1]'), TO_VECTOR('[2]')),
                      (99, NULL, NULL);
CREATE TABLE t_metric_name (id INT PRIMARY KEY, name VARCHAR(10));
INSERT INTO t_metric_name VALUES (1, "EUCLIDEAN"), (99, NULL);

--echo #
--echo # 1) Test how different number and types of arguments are handled.
--echo #
--echo # 1.1) Arity.
--echo #
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT DISTANCE();
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT DISTANCE(TO_VECTOR("[1]"));
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT DISTANCE(TO_VECTOR("[1]"), TO_VECTOR("[2]"));
eval SELECT DISTANCE(TO_VECTOR("[1]"), TO_VECTOR("[2]"), "$metric");
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT DISTANCE(TO_VECTOR("[1]"), TO_VECTOR("[2]"), TO_VECTOR("[3]"), "EUCLIDEAN");

--echo #
--echo # 1.2) Argument types.
--echo #
--echo # Only vectors or binary strings are allowed for first the two arguments.
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE("[1]", TO_VECTOR("[2]"), "$metric");
eval SELECT DISTANCE(X'0000803F', TO_VECTOR("[2]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0]"), TO_VECTOR("[2]"), "$metric");
eval SELECT DISTANCE(v1, TO_VECTOR("[2]"), "$metric") FROM t1 WHERE id = 0;
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(id, TO_VECTOR("[2]"), "$metric") FROM t1 WHERE id = 0;
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[1]"), "[2]", "$metric");
eval SELECT DISTANCE(TO_VECTOR("[2]"), X'0000803F', "$metric");
eval SELECT DISTANCE(TO_VECTOR("[3]"), v1, "$metric") FROM t1 WHERE id = 1;
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0]"), id, "$metric") FROM t1 WHERE id = 1;

--echo # The third argument must be a string literal with value from the
--echo # fixed list of metric names.
--error ER_WRONG_ARGUMENTS
SELECT DISTANCE(TO_VECTOR("[1, 0]"), TO_VECTOR("[-1, 0]"), 1);
--error ER_WRONG_ARGUMENTS
SELECT DISTANCE(TO_VECTOR("[1, 1]"), TO_VECTOR("[1, 0]"), CONCAT("EUCLI","DEAN"));
SELECT DISTANCE(TO_VECTOR("[1, 1]"), TO_VECTOR("[2, 0]"), "euclidean");
SELECT DISTANCE(TO_VECTOR("[1, 1]"), TO_VECTOR("[3, 0]"), "EuClIdEaN");
SELECT DISTANCE(TO_VECTOR("[1, 1]"), TO_VECTOR("[4, 0]"), X'4555434C494445414E');
--echo # Metric strings with embedded NUL must be rejected regardless of prefix match.
--error ER_WRONG_ARGUMENTS
SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[1, 0]"), X'4555434C494445414E00');
--error ER_WRONG_ARGUMENTS
SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[1, 0]"), X'4555434C494445414E004A554E4B');
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[5, 0]"), "NOSUCHMETRIC");
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[6, 0]"), name) FROM t_metric_name WHERE id = 1;
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[7, 0]"), NULL);

--echo #
--echo # 1.3) NULL arguments and nullability in metadata for result.
--echo #
eval SELECT DISTANCE(NULL, TO_VECTOR("[1, 0]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[1, 1]"), NULL, "$metric");
eval SELECT DISTANCE(v2, TO_VECTOR("[1, 0]"), "$metric") FROM t1 WHERE id = 99;
eval SELECT DISTANCE(TO_VECTOR("[1, 1]"), v2, "$metric") FROM t1 WHERE id = 99;
--echo # The third argument doesn't allow NULL values in any form.
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[1, 0]"), NULL);
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[1, 0]"), name) FROM t_metric_name WHERE id = 99;
--echo # The result metadata should indicate that it is nullable.
eval CREATE TABLE tt SELECT DISTANCE(TO_VECTOR("[1, 0]"), TO_VECTOR("[0, 1]"), "$metric") AS d;
SHOW CREATE TABLE tt;
DROP TABLE tt;

--echo #
--echo # 2) Test vector arguments length mismatch.
--echo #
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[1]"), TO_VECTOR("[1, 0]"), "$metric");
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(v2, TO_VECTOR("[1]"), "$metric") FROM t1 WHERE id = 1;
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(v1, v2, "$metric") FROM t1 WHERE id = 1;
--echo #
--echo # Note that length check happens at runtime. This is well visible
--echo # when we have value stored in a vector field which is shorter than
--echo # maximum length specified at the field creation time.
eval SELECT DISTANCE(v1, v2, "$metric") FROM t1 WHERE id = 98;
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), v2, "$metric") FROM t1 WHERE id = 98;
--echo # Binary-string BLOB arguments exceeding max_dimensions (16383) are rejected.
--echo # A BLOB column is used so the argument passes the resolve-time binary-charset
--echo # type check; the max_dimensions guard fires at runtime in val_real().
CREATE TABLE t_oversized (v MEDIUMBLOB);
INSERT INTO t_oversized VALUES (REPEAT(X'00000000', 16384));
--error ER_WRONG_ARGUMENTS
eval SELECT DISTANCE(v, v, "$metric") FROM t_oversized;
DROP TABLE t_oversized;

--echo #
--echo # 3) Some basic tests for different (from syntax PoV) variants of
--echo #    arguments.
--echo #
eval SELECT DISTANCE(X'0000803F0000803F', X'0000000000000040', "$metric");
eval SELECT DISTANCE(X'0000803F0000803F', TO_VECTOR("[2, 0]"), "$metric");
eval SELECT DISTANCE(X'0000803F0000803F', v2, "$metric") FROM t1 WHERE id = 4;
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[1, 0]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), X'000000000000803F', "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), v2, "$metric") FROM t1 WHERE id = 1;
eval SELECT DISTANCE(a.v2, b.v2, "$metric") FROM t1 AS a, t1 AS b WHERE a.id = 0 AND b.id = 4;
eval SELECT DISTANCE(v2, X'0000000000000040', "$metric") FROM t1 WHERE id = 0;
eval SELECT DISTANCE(v2, TO_VECTOR("[0, 2]"), "$metric") FROM t1 WHERE id = 0;
--echo # Non-trivial (artificial) combinations
eval SELECT DISTANCE(TO_VECTOR(CONCAT("[0", ", ", "1]")), CONCAT(X'00000000', X'00000040'), "$metric");
--echo # The below case demonstrates that arguments to DISTANCE might not be
--echo # well-aligned in memory.
eval SELECT DISTANCE(SUBSTR(X'010000000000000040', 2), RIGHT(X'40000000000000803F', 8), "$metric");
--echo # 9-byte blobs; SUBSTR from pos 2 → 8 bytes at offset 1 (misaligned for float).
--echo # Length must stay a multiple of 4; SUBSTR(..., 4) on 9 bytes yields 6 → ER_TO_VECTOR_CONVERSION.
eval SELECT DISTANCE(SUBSTR(X'000100000000000040', 2), SUBSTR(X'00040000000000803F', 2), "$metric");

--echo #
--echo # 4) Basic test for different vector values.
--echo #
--echo # Identical / collinear vectors.
eval SELECT DISTANCE(TO_VECTOR("[1, 1]"), TO_VECTOR("[1, 1]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[1, 0]"), TO_VECTOR("[1, 0]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[1, 1]"), TO_VECTOR("[2.5, 2.5]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[1, 2, 3, 4, 5]"), TO_VECTOR("[1, 2, 3, 4, 5]"), "$metric");
--echo # Orthogonal vectors.
eval SELECT DISTANCE(TO_VECTOR("[1, 0]"), TO_VECTOR("[0, 1]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 1, 0]"), TO_VECTOR("[-1, 0, -1]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[1, 0, 3, 0, 5]"), TO_VECTOR("[0, 2, 0, 4, 0]"), "$metric");
--echo # Anti-parallel vectors.
eval SELECT DISTANCE(TO_VECTOR("[-1, -1]"), TO_VECTOR("[2, 2]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[-2e38, 1]"), TO_VECTOR("[2e38, -1]"), "$metric");
--echo # Distance from origin.
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[1, 0]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[3, 4]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[5, 12]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 0, 0, 0]"), TO_VECTOR("[1, 1, 1, 1]"), "$metric");
--echo # Mixed-sign and larger vectors.
eval SELECT DISTANCE(TO_VECTOR("[1, 2, 3]"), TO_VECTOR("[4, 5, 6]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[1, 7, 3, 16, 5]"), TO_VECTOR("[1, 2, 3, 4, 5]"), "$metric");
--echo # Zero vector (behavior differs per metric).
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[2, 2]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[0, 0]"), "$metric");
eval SELECT DISTANCE(TO_VECTOR("[0]"), TO_VECTOR("[0]"), "$metric");
--echo # Large values near float32 max.
eval SELECT DISTANCE(TO_VECTOR("[0, 0]"), TO_VECTOR("[2e38, 0]"), "$metric");
--echo # Same value in a 16-dim vector (one full 16-lane block): squaring 2e38
--echo # overflows float32, so the sums must be done in double.
eval SELECT DISTANCE(TO_VECTOR("[2e38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]"),
                     TO_VECTOR("[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]"),
                     "$metric");
--echo # Element difference exceeds FLT_MAX (2.5e38 - (-2.5e38) = 5e38): the
--echo # subtraction must be done in double, not float32, for every metric.
eval SELECT DISTANCE(STRING_TO_VECTOR('[2.5e38,2.5e38,2.5e38,2.5e38,2.5e38,2.5e38,2.5e38,2.5e38]'),
                     STRING_TO_VECTOR('[-2.5e38,-2.5e38,-2.5e38,-2.5e38,-2.5e38,-2.5e38,-2.5e38,-2.5e38]'),
                     "$metric");
--echo # Same overflow in the last element only.
eval SELECT DISTANCE(TO_VECTOR("[1, 1, 1, 1, 2.5e38]"),
                     TO_VECTOR("[0, 0, 0, 0, -2.5e38]"), "$metric");
--echo # Tiny values against their negation: 1e-23 * 1e-23 is 0 in float32, so
--echo # the sums must be done in double.
eval SELECT DISTANCE(TO_VECTOR("[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23]"),
                     TO_VECTOR("[-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23]"), "$metric");
--echo # 20 dims: only the first 16 elements hold the tiny values.
eval SELECT DISTANCE(TO_VECTOR("[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,0,0,0,0]"),
                     TO_VECTOR("[-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,0,0,0,0]"), "$metric");
--echo # 16383 dims (the maximum), elements near 1e-21: products are subnormal
--echo # in float32, so they are recomputed in double (Manhattan has no products
--echo # and stays in float32).  Every SIMD width sums in the same order, so the
--echo # result is the same on every CPU.
eval SELECT DISTANCE(TO_VECTOR(CONCAT('[', REPEAT('1e-21,', 16382), '1e-21]')),
                     TO_VECTOR(CONCAT('[', REPEAT('2e-21,', 16382), '2e-21]')),
                     "$metric");
--echo # Subnormal float32 inputs (TO_VECTOR rejects them, binary arguments do
--echo # not): 16 x 1e-40 against 16 x -1e-40.  Products flush to zero in
--echo # float32, so they are recomputed in double.
eval SELECT DISTANCE(X'C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100C2160100',
                     X'C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180C2160180', "$metric");
--echo # Symmetry: DISTANCE(a, b) = DISTANCE(b, a).
eval SELECT DISTANCE(TO_VECTOR("[1, 2, 3]"), TO_VECTOR("[4, 5, 6]"), "$metric") =
          DISTANCE(TO_VECTOR("[4, 5, 6]"), TO_VECTOR("[1, 2, 3]"), "$metric");
--echo # Special IEEE 754 float32 values: NaN, +Infinity, -Infinity.
--echo # NaN/Inf input elements raise ER_DATA_OUT_OF_RANGE for all metrics (POW/EXP convention).
--error ER_DATA_OUT_OF_RANGE
eval SELECT DISTANCE(X'0000C07F', X'00000000', "$metric");
--error ER_DATA_OUT_OF_RANGE
eval SELECT DISTANCE(X'0000807F', X'00000000', "$metric");
--error ER_DATA_OUT_OF_RANGE
eval SELECT DISTANCE(X'000080FF', X'00000000', "$metric");
--echo # Lane layout coverage: elements are summed in 16-lane blocks plus a tail.
--echo # 16-dim: one block, no tail.
eval SELECT DISTANCE(TO_VECTOR("[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0]"),
                     TO_VECTOR("[0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1]"),
                     "$metric");
--echo # 20-dim: one block and a 4-element tail.
eval SELECT DISTANCE(TO_VECTOR("[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0]"),
                     TO_VECTOR("[0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1]"),
                     "$metric");

--echo #
--echo # prepared statements
--echo #
eval PREPARE s1 FROM 'SELECT DISTANCE(v1, TO_VECTOR("[2]"), "$metric") FROM t1 WHERE id = 4';
EXECUTE s1;
EXECUTE s1;
DEALLOCATE PREPARE s1;

--error ER_WRONG_ARGUMENTS
PREPARE stmt FROM 'SELECT DISTANCE(v1, TO_VECTOR("[2]"), "?") FROM t1 WHERE id = 4';

eval PREPARE stmt FROM 'SELECT DISTANCE(v1, TO_VECTOR(?), "$metric") FROM t1 WHERE id = 4';
SET @met="[2]";
EXECUTE stmt USING @met;
DEALLOCATE PREPARE stmt;

--disable_warnings
--echo #
--echo # 5) Distance in query contexts.
--echo #
--echo # ORDER BY distance: nearest-neighbour pattern.
eval SELECT id FROM t1 WHERE id IN (0,1,2,3,4)
     ORDER BY DISTANCE(v2, TO_VECTOR('[1, 0]'), "$metric"), id;
--echo # ORDER BY distance DESC: farthest-neighbour pattern.
eval SELECT id FROM t1 WHERE id IN (0,1,2,3,4)
     ORDER BY DISTANCE(v2, TO_VECTOR('[1, 0]'), "$metric") DESC, id;
--echo # WHERE: range query filtering by distance.
eval SELECT id FROM t1
     WHERE id IN (0,1,2,3,4) AND DISTANCE(v2, TO_VECTOR('[1, 0]'), "$metric") < 1.5
     ORDER BY id;
--echo # Derived table with distance.
eval SELECT id FROM
     (SELECT id, DISTANCE(v2, TO_VECTOR('[1, 0]'), "$metric") AS d
      FROM t1 WHERE id IN (0,1,2,3,4)) AS sq
     WHERE d IS NOT NULL ORDER BY d, id;
--enable_warnings

if ($metric == COSINE)
{
--echo # Zero-vector cosine in DML: must insert NULL without aborting under strict sql_mode.
CREATE TABLE tt_cosine_dml (d DOUBLE);
INSERT INTO tt_cosine_dml SELECT DISTANCE(TO_VECTOR('[0]'), TO_VECTOR('[0]'), 'COSINE');
SELECT d FROM tt_cosine_dml;
DROP TABLE tt_cosine_dml;

--echo # Float32 underflow must not be mistaken for a zero vector.  Squaring
--echo # 1e-23 in float32 gives 0, which collapsed float32 norm accumulators and
--echo # returned NULL (the zero-vector sentinel) for non-zero vectors, only at
--echo # some dims, so the result depended on the CPU.
--echo # A vector against itself is distance 0.
SELECT DISTANCE(
  TO_VECTOR('[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23]'),
  TO_VECTOR('[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23]'),
  'COSINE') AS tiny_self;
--echo # Opposite direction is distance 2.
SELECT DISTANCE(
  TO_VECTOR('[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23]'),
  TO_VECTOR('[-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23,-1e-23]'),
  'COSINE') AS tiny_anti;
--echo # A genuine 16-dim zero vector is still NULL: the sentinel is intact.
SELECT DISTANCE(
  TO_VECTOR('[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]'),
  TO_VECTOR('[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23]'),
  'COSINE') AS tiny_vs_zero;
--echo # 20 dims: tiny values in the 16-element block and in the tail.  Expected
--echo # 1 - 1/sqrt(17).
SELECT DISTANCE(
  TO_VECTOR('[1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,1e-23,0,0,0]'),
  TO_VECTOR('[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1e-23,0,0,0]'),
  'COSINE') AS tiny_head_tail;
--echo # Partial underflow: float32 rounds (5e-23)^2, (1e-22)^2 and their
--echo # product to 2, 7 and 4 subnormal steps, which gave a negative distance
--echo # (-0.069) for parallel vectors.  Expected 0.
SELECT DISTANCE(
  TO_VECTOR('[5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23,5e-23]'),
  TO_VECTOR('[1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22,1e-22]'),
  'COSINE') AS tiny_parallel;
}

DROP TABLE t_metric_name;
DROP TABLE t1;
