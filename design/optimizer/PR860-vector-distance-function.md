# Bug#121547: DISTANCE() and VECTOR_DISTANCE() for the VECTOR data type

- Primary issue: mysql/mysql-community#42
- Related issues: #751, #708
- Bug DB: [Bug #121547](https://bugs.mysql.com/bug.php?id=121547) (sub-request of [Bug #120408](https://bugs.mysql.com/bug.php?id=120408))
- Design review and implementation PR: #860

## 1. High Level Design

### Executive Summary

This specification adds the SQL function `DISTANCE(a, b, metric)` and its
synonym `VECTOR_DISTANCE(a, b, metric)`, which return the distance between two
vectors as a `DOUBLE` for the metrics `EUCLIDEAN`, `EUCLIDEAN_SQUARED`,
`MANHATTAN`, `COSINE` and `DOT`, so that users can run exact nearest-neighbour
(k-NN) searches with `ORDER BY DISTANCE(...) LIMIT k`. The function uses
Single Instruction Multiple Data (SIMD) CPU instructions where available and
returns bit-identical results on every CPU, so stored distances stay valid
when data moves between machines.

### Motivation and User Stories

The `VECTOR` data type can be stored and converted (`TO_VECTOR()`,
`FROM_VECTOR()`, `VECTOR_DIM()`), but two vectors cannot be compared in SQL.
The type is therefore storage-only: similarity search, the main reason to
store embeddings, requires exporting the data to another system. MariaDB and
PostgreSQL (pgvector) already provide distance functions
(mysql/mysql-community#42).

A distance function is the base of every vector search feature. Exact k-NN
search needs only this function. A future vector index (approximate nearest
neighbour, ANN) needs the same metrics and the same results.

- As an application developer storing embeddings in a `VECTOR` column, I want
  to compute the distance between a query vector and each stored vector, so
  that I can return the k most similar rows with a single SQL query.
- As an application developer, I want every metric to rank with
  `ORDER BY ... ASC`, so that I can switch metrics without rewriting queries.
- As a database administrator, I want `DISTANCE()` to return the same value on
  every server, so that distances stored in generated columns and indexes
  stay valid when I move or replicate tables between machines with different
  CPUs.
- As a database administrator, I want to know whether the server uses SIMD
  instructions for vector distances, so that I can explain performance
  differences between hosts.

### Scope

In scope:

- The native functions `DISTANCE()` and `VECTOR_DISTANCE()` in the SQL layer
  (`sql/`).
- Distance kernels for x86_64 (SSE4.2, AVX2, AVX-512F), aarch64 (NEON) and a
  portable scalar implementation (`vector-common/`).
- Runtime selection of the kernel at server startup, and one information
  message in the error log.

Out of scope:

- Vector indexes and approximate nearest-neighbour search.
- The metrics `HAMMING` and `JACCARD`, `VECTOR_NORM()`, vector arithmetic and
  aggregate functions (#751).
- The SQL standard syntax with the metric as a keyword (see "Differences
  from the SQL standard").
- `VECTOR` element types other than float32.
- Allowing `VECTOR` columns in generated columns, functional indexes, `DEFAULT`
  expressions and `CHECK` constraints. The server rejects them since WL#16081;
  this specification does not change that. `DISTANCE()` values can be stored
  when the vectors are kept in binary-string columns.
- SIMD kernels on Windows, and SVE/SVE2 kernels on aarch64.
- Optimizer changes: `DISTANCE()` is evaluated row by row like any other
  scalar function.
- Connectors, MySQL Shell and other tools.

### Related Work

- The section "SQL Standard (Draft) Conformance" specifies the MySQL function
  in the same form and lists the differences.
- #751 "[VI] Syntax and functions for using vectors" proposes
  `VECTOR_DISTANCE(vector1, vector2, metric)` with the standard's metrics.
  This specification implements all of them except `HAMMING`.
- MySQL HeatWave provides `DISTANCE()` and `VECTOR_DISTANCE()`. This
  specification uses the same names so that queries are portable.
- #708 "[VI] MySQL Native Vector Index": a vector index needs a distance
  function. The kernels in `vector-common/` can be reused by such an index.

### References

- IEEE 754-2019, Standard for Floating-Point Arithmetic
- WL#16081: Native Vector Embeddings Support In HeatWave (introduced the
  `VECTOR` type and its restrictions)

## 2. Requirements

### Functional Requirements

FR1. The server MUST provide the native function `DISTANCE(a, b, metric)` and
the synonym `VECTOR_DISTANCE(a, b, metric)` with identical behavior.

FR2. Each of `a` and `b` MUST be a `VECTOR` expression or a binary string
(`my_charset_bin`) of packed float32 values in the server's native byte
order, the layout that `TO_VECTOR()` returns and `FROM_VECTOR()` accepts. All
supported platforms are little-endian. Any other type MUST fail with
`ER_WRONG_ARGUMENTS` when the statement is prepared.

FR3. `metric` MUST be a constant string literal with one of the values
`EUCLIDEAN`, `EUCLIDEAN_SQUARED`, `MANHATTAN`, `COSINE` or `DOT`, compared
case-insensitively. A non-constant expression, a `NULL`, a numeric value or an
unknown name MUST fail with `ER_WRONG_ARGUMENTS` when the statement is
prepared.

FR4. Calling the function with other than three arguments MUST fail with
`ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT`.

FR5. The function MUST return `DOUBLE` and MUST be nullable in result
metadata (for example in `CREATE TABLE ... SELECT`).

FR6. For vectors `a` and `b` with `n` elements, the function MUST return:

| Metric              | Result                                   | Range       |
| ------------------- | ---------------------------------------- | ----------- |
| `EUCLIDEAN`         | sqrt(sum((a[i] - b[i])^2))               | [0, +inf)   |
| `EUCLIDEAN_SQUARED` | sum((a[i] - b[i])^2)                     | [0, +inf)   |
| `MANHATTAN`         | sum(\|a[i] - b[i]\|)                     | [0, +inf)   |
| `COSINE`            | 1 - sum(a[i]·b[i]) / sqrt(sum(a[i]^2) · sum(b[i]^2)) | [0, 2] |
| `DOT`               | -sum(a[i]·b[i])                          | (-inf, +inf) |

A smaller value MUST always mean "closer", so that every metric ranks with
`ORDER BY ... ASC`. For this reason `DOT` returns the negated inner product.
These are the formulas of the SQL standard; see "Differences from the SQL
standard".

FR7. `COSINE` MUST return `NULL` when either vector has all elements equal to
zero. The `COSINE` result MUST be clamped to [0, 2], so that rounding cannot
produce a value outside the range of `1 - cos`.

FR8. If `a` or `b` is `NULL`, the function MUST return `NULL`.

FR9. If `a` and `b` have a different number of dimensions, the function MUST
fail with `ER_WRONG_ARGUMENTS`. This check happens per row, at execution time.

FR10. If a binary string argument has a length that is not a multiple of 4,
the function MUST fail with `ER_TO_VECTOR_CONVERSION`. If it has more than
16383 (`max_dimensions`) elements, the function MUST fail with
`ER_WRONG_ARGUMENTS`.

FR11. If the result is not finite (an input element is NaN or ±Inf), the
function MUST fail with `ER_DATA_OUT_OF_RANGE`. This MUST also hold when `a`
and `b` are byte-identical.

FR12. For the same arguments the function MUST return a bit-identical result
on every supported CPU, SIMD tier, operating system and compiler. Values
stored in stored generated columns, and index entries of indexed virtual
generated columns and functional indexes, MUST remain valid when the server
runs on another tier. These objects cannot reference `VECTOR` columns
(`ER_INCORRECT_TYPE`); they can reference binary-string columns.

FR13. The function MUST accept argument data at any memory alignment.

FR14. Statements that call the function MUST be marked unsafe for
statement-based binary logging (`BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION`).

FR15. At startup the server MUST select the fastest kernel the CPU supports
and MUST write one information-level message naming it to the error log.

### Non-functional Requirements

NFR1. Queries that do not call `DISTANCE()` MUST NOT be affected. The only
added cost is one CPU feature check at startup.

NFR2. The function MUST NOT allocate memory per call beyond what evaluating
its arguments already allocates. Kernels use only stack memory.

NFR3. On x86_64 with AVX2, the selected kernel SHOULD be faster than
the scalar kernel for vectors larger than 128 dimensions. Below 16
dimensions every tier runs the same scalar tail code, and the SIMD kernels
MUST NOT be slower than the scalar kernel beyond noise.

NFR4. The float32 fast path MUST be used for normally-scaled data. The double
precision fallback (see 4.3) MUST be used only when a float32 lane sum is
non-finite or smaller than `(max_dimensions + 1) * FLT_MIN`.

## 3. High Level Specification

### Technical Summary

`Item_func_vector_distance` is a new `Item_real_func`. `resolve_type()`
validates the argument types and the metric name once per statement and
stores the metric. `val_real()` evaluates both arguments, checks their
lengths, calls one of four kernels and applies the metric-specific final step
(square root for `EUCLIDEAN`, negation for `DOT`, `NULL` for an undefined
`COSINE`).

The kernels live in `vector-common/vector_distance.{h,cc}`. They are written
once as templates using GCC/Clang vector extensions and instantiated per SIMD
tier with a per-function `target` attribute, so the server does not need
`-march` flags. At startup `init_vector_distance_functions()` uses
`__builtin_cpu_supports()` to choose the best tier and sets four function
pointers.

Results are bit-identical on every tier because the order of floating-point
operations does not depend on register width:

- All tiers accumulate into the same 16 float32 lanes. Element `i` goes into
  lane `i % 16`. Tiers differ only in how many registers hold the 16 lanes
  (1 for AVX-512F, 2 for AVX2, 4 for SSE4.2 and NEON, 16 scalars).
- The 16 lane sums are converted to double and added in a fixed binary tree.
- The remaining `dims % 16` elements are added in index order in double.
- Fused multiply-add is disabled for `vector_distance.cc`: GCC and Clang get
  `-ffp-contract=off`, MSVC gets `#pragma fp_contract(off)`. A fused
  multiply-add rounds once instead of twice and would make FMA-capable tiers
  differ.
- When a float32 lane sum is non-finite or below
  `(max_dimensions + 1) * FLT_MIN`, overflow or subnormal products may have
  made it inexact. The kernel then recomputes the distance with the lanes in
  double, where products of float32 values are exact. This decision depends
  only on the float32 lane sums, which are identical on every tier.

#### Alternatives Considered

- **Accumulate in double on every tier.** Exact for products, but each
  register holds half as many lanes and every element needs a conversion.
  Float32 accumulation with an exact fallback keeps full SIMD width for
  normally-scaled data and is still identical on every tier.
- **Let each tier use its natural reduction order.** Fastest, but different
  CPUs then return different values (for example `DOT` of
  `[1e4, 1e4, 5, 0...]` and `[1e4, -1e4, 1, 0...]` gives -5 or -8). A stored
  generated column or index entry would then depend on the machine that wrote
  it, so this was rejected.
- **Compile-time ISA selection (`-march`).** Packages must run on any CPU of
  the target architecture, so runtime dispatch is required.
- **Intrinsics per tier.** GCC/Clang vector extensions let one template serve
  all tiers, so there is one implementation to review and test instead of
  five.
- **SVE/SVE2 kernels.** SVE's vector length is only known at runtime and does
  not map onto a fixed 16-lane layout. NEON is baseline on aarch64 and is used
  on SVE hosts.
- **Return the positive inner product for `DOT`.** Larger would then mean
  closer, and `DOT` would need `ORDER BY ... DESC` unlike every other metric.
  Returning the negated value keeps one ranking direction for all metrics,
  and is what the SQL standard specifies.
- **Metric as a keyword, as in the SQL standard.** It needs grammar rules and
  new keywords in `sql/sql_yacc.yy`. A string literal keeps the function an
  ordinary native function registered in `sql/item_create.cc`. The keyword
  form can be added later as a grammar rule that builds the same item.
- **Allow a non-literal metric.** It would force a per-row metric lookup and
  prevent the optimizer from knowing the metric. It has no practical use case.

### User Interface

#### Controls

Not applicable. No system variables, command-line options or statements are
added. The SIMD tier is selected automatically and cannot be changed.

Debug builds only: the debug keyword `vector_distance_force_scalar`
(`--debug=+d,vector_distance_force_scalar`) selects the scalar tier at
startup. Tests use it to simulate moving data to a machine with another CPU.

#### Observability

New error log message:

- Code: `ER_VECTOR_DISTANCE_SIMD_DISPATCH`
- Component: Server
- Level: Information (`INFORMATION_LEVEL`), so it is visible only with
  `log_error_verbosity=3`.
- Text: `For VECTOR_DISTANCE using %s`, where `%s` is
  `hardware accelerated <tier>.` (`<tier>` is `AVX-512F`, `AVX2`, `SSE4.2`
  or `NEON`) or `software scalar.`. Example:
  `For VECTOR_DISTANCE using hardware accelerated AVX2.`
- It is written once at startup and is visible in
  `performance_schema.error_log`.

No status variables, Performance Schema tables, instruments, mutexes, threads
or thread stages are added.

#### Usage

```sql
CREATE TABLE docs (id INT PRIMARY KEY, emb VECTOR(3));
INSERT INTO docs VALUES (1, TO_VECTOR('[1, 0, 0]')),
                        (2, TO_VECTOR('[0.9, 0.1, 0]')),
                        (3, TO_VECTOR('[0, 1, 0]'));

-- Exact k-NN search
SELECT id, DISTANCE(emb, TO_VECTOR('[1, 0, 0]'), 'COSINE') AS d
FROM docs ORDER BY d LIMIT 2;

-- Synonym, other metric
SELECT id FROM docs
ORDER BY VECTOR_DISTANCE(emb, TO_VECTOR('[1, 0, 0]'), 'EUCLIDEAN') LIMIT 2;

-- Binary string arguments (packed native-order float32; little-endian here)
SELECT DISTANCE(X'0000803F', TO_VECTOR('[2]'), 'MANHATTAN');   -- 1
```

Storing distances. Generated columns and functional indexes cannot reference
a `VECTOR` column, so the vectors are kept in a binary-string column:

```sql
CREATE TABLE docs_bin (
  id INT PRIMARY KEY,
  emb VARBINARY(12),                      -- 3 x float32
  d_ref DOUBLE AS (DISTANCE(emb, TO_VECTOR('[1, 0, 0]'), 'EUCLIDEAN')) STORED,
  INDEX i_ref (d_ref),
  INDEX i_dot ((DISTANCE(emb, TO_VECTOR('[1, 0, 0]'), 'DOT'))));
INSERT INTO docs_bin (id, emb) SELECT id, emb FROM docs;

-- Uses i_dot. The constant must be a DOUBLE (0e0, not 0) for the optimizer
-- to match the functional index.
SELECT id FROM docs_bin
WHERE DISTANCE(emb, TO_VECTOR('[1, 0, 0]'), 'DOT') < 0e0;
```

Grammar: no change. `DISTANCE` and `VECTOR_DISTANCE` are registered as native
functions in `sql/item_create.cc` and are not reserved words.

#### SQL Standard (Draft) Conformance

**Function**

Determine the distance between two vectors.

**Format**

```text
<vector distance function> ::=
    <vector distance function name> <left paren> <vector 1> <comma>
    <vector 2> <comma> <vector distance metric> <right paren>

<vector distance function name> ::=
    DISTANCE
  | VECTOR_DISTANCE

<vector 1> ::=
    <value expression>

<vector 2> ::=
    <value expression>

<vector distance metric> ::=
    <character string literal>
```

**Syntax Rules**

1. The declared type of the result of `<vector distance function>` VDF is
   `DOUBLE`. VDF is possibly nullable.
2. The declared type of the `<value expression>` immediately contained in
   `<vector 1>` and in `<vector 2>` MUST be `VECTOR`, or a character string
   type with the binary character set. Otherwise, an error is raised:
   `ER_WRONG_ARGUMENTS`. The two declared types need not be the same.
3. If `<vector 1>` or `<vector 2>` is a `<dynamic parameter specification>`,
   its declared type is `VECTOR`.
4. The `<vector distance metric>` MUST be a constant. Its value, compared
   case-insensitively, MUST be one of `'EUCLIDEAN'`, `'EUCLIDEAN_SQUARED'`,
   `'MANHATTAN'`, `'COSINE'` or `'DOT'`. Otherwise, an error is raised:
   `ER_WRONG_ARGUMENTS`. A `<dynamic parameter specification>` is not allowed.

##### Differences from the SQL standard

| # | Topic | SQL standard | MySQL |
| - | ----- | ------------------- | ----- |
| 1 | Function name | `VECTOR_DISTANCE` | `VECTOR_DISTANCE`, and `DISTANCE` as an extension. |
| 2 | Metric syntax | A keyword: `VECTOR_DISTANCE(v1, v2, COSINE)` | A string literal: `VECTOR_DISTANCE(v1, v2, 'COSINE')`. A bare keyword is parsed as a column reference and fails with `ER_BAD_FIELD_ERROR`, or with `ER_WRONG_ARGUMENTS` if such a column exists. **Not conforming.** |
| 3 | `HAMMING` | Supported | Not supported: `ER_WRONG_ARGUMENTS`. |
| 4 | Argument types | Vector value expressions with the same declared type | `VECTOR` or a binary string. Declared types may differ (`VECTOR(2)` and `VECTOR(4)` are accepted together); only the actual number of coordinates must match. |
| 5 | Dimension mismatch | A syntax error, since the declared types differ | A runtime error per row: `ER_WRONG_ARGUMENTS`. |
| 6 | Result type | Implementation-defined approximate numeric type | `DOUBLE`. Conforming. |
| 7 | Precision | The exact value, converted to the result type | The sums are accumulated in float32 lanes with a double fallback (see the Technical Summary), so the result can differ from the correctly rounded exact value by float32 rounding error. The result is reproducible on every platform. |
| 8 | Out-of-range intermediates | An error if any intermediate result does not fit its type | Intermediate results that overflow float32 are recomputed in double, so they raise no exception (`DISTANCE(TO_VECTOR('[3e38]'), TO_VECTOR('[-3e38]'), 'EUCLIDEAN_SQUARED')` returns 3.6e77). `ER_DATA_OUT_OF_RANGE` is raised only when the result is NaN or infinite, which needs a NaN or infinite coordinate. |
| 9 | Non-finite coordinates | Not applicable: coordinates are numbers | A binary-string argument can hold NaN or ±Inf; the result is then `ER_DATA_OUT_OF_RANGE`. |
| 10 | `COSINE` of a zero vector | R = 1 − Σ(Ai·Bi) / (√Σ(Ai²) · √Σ(Bi²)). The denominator is 0, so R is undefined; the standard gives no special case. | The null value. |
| 11 | `COSINE` denominator and range | √Σ(Ai²) · √Σ(Bi²); the exact value of R is always in [0, 2] | √(Σ(Ai²) · Σ(Bi²)): mathematically equal, one square root instead of two, rounds differently in the last bit. The result is clamped to [0, 2] so that rounding cannot leave the range. Equivalent apart from rounding (see row 7). |
| 12 | `DOT` sign | R = −Σ(Ai·Bi) | R = −Σ(Ai·Bi). Conforming. |
| 13 | Maximum dimension | Implementation-defined | 16383 (`max_dimensions` of the `VECTOR` type). |

### Security Context

Not applicable. No privileges are added and the function reads only its
arguments.

### System Resources

Not applicable. No additional memory, threads or files are used (NFR1, NFR2).

### Deployment and Installation

Not applicable.

### Protocols

Not applicable.

### Failure Model

Not applicable.

### Upgrade/Downgrade

Not applicable.

### Cross-version Replication

- Every tier returns the same bits, so source and replica hardware do not
  matter. A replica running another server version, however, may not have the
  function, or may compute it in a different order and round differently.
  Statements that call it are therefore marked unsafe (FR14).
- With `binlog_format=ROW` (default) or `MIXED`, such statements are logged
  as row events, so the replica applies the computed values and does not need
  the function, unless the function appears in DDL (generated column,
  functional index, view).
- With `binlog_format=STATEMENT`, they raise the unsafe-statement note
  `ER_BINLOG_UNSAFE_STATEMENT`.

## 4. Low Level Design

### 4.1 Block Diagram

```text
 mysqld startup (init_server_components)
   │
   ├─ init_vector_distance_functions()
   │     __builtin_cpu_supports → AVX-512F | AVX2 | SSE4.2 | NEON | scalar
   │     sets g_euclidean, g_cosine, g_dot_product, g_manhattan
   │     (debug keyword vector_distance_force_scalar → scalar)
   │
   └─ LogErr(INFORMATION_LEVEL, ER_VECTOR_DISTANCE_SIMD_DISPATCH, ...)

 SQL: DISTANCE(a, b, 'METRIC')
   │
   ├─ item_create.cc: DISTANCE / VECTOR_DISTANCE → Item_func_vector_distance
   ├─ do_itemize():   mark statement unsafe for SBR
   ├─ resolve_type(): check arg types, parse metric literal → m_metric
   └─ val_real():     read a, b → check lengths/dims
                        │
                        ▼
        vector-common/vector_distance.cc
        vector_distance_{euclidean_squared,cosine,dot,manhattan}()
                        │  function pointer set at startup
                        ▼
        <metric>_<tier>()  = lanes_kernel<Lanes, Metric>
            16 float32 lanes → fixed double tree → tail in double
            └─ out of range? → double_lanes_kernel<Lanes, Metric>
                        │
                        ▼
        sqrt (EUCLIDEAN) | negate (DOT) | +Inf → NULL (COSINE)
        check_float_overflow → ER_DATA_OUT_OF_RANGE
```

### 4.2 Interface Specification

New header `vector-common/vector_distance.h`:

```cpp
/** Set SIMD function pointers to the best tier for this CPU.
    Called once at server startup; safe to call repeatedly. */
void init_vector_distance_functions();

/** SIMD tier identifiers. Every tier returns bit-identical results. */
enum class VectorDistanceTier { Scalar = 0, Sse42, Avx2, Avx512f, Neon };

/** @return true when @p tier can run on this CPU and build. */
bool vector_distance_tier_available(VectorDistanceTier tier);

/** Force @p tier (tests, benchmarks and the debug keyword only).
    @pre vector_distance_tier_available(tier) */
void init_vector_distance_functions_tier(VectorDistanceTier tier);

/** @return the tier selected by the last init call. */
VectorDistanceTier vector_distance_tier();

/** @return a stable label, e.g. "AVX-512F", "software scalar". */
const char *vector_distance_tier_label(VectorDistanceTier tier);

/** Write the dispatch summary for ER_VECTOR_DISTANCE_SIMD_DISPATCH, e.g.
    "hardware accelerated AVX2."
    @return bytes written, excluding the terminating NUL. */
size_t vector_distance_dispatch_description(char *buf, size_t buf_len);

/** @return sum((a[i]-b[i])^2). @p a and @p b may have any alignment. */
double vector_distance_euclidean_squared(const char *a, const char *b,
                                         uint32_t dims);

/** @return 1 - cos(a, b) clamped to [0, 2]; +Inf when either vector is all
    zeros; NaN when an element is NaN or Inf. */
double vector_distance_cosine(const char *a, const char *b, uint32_t dims);

/** @return sum(a[i]*b[i]) (not negated). */
double vector_distance_dot(const char *a, const char *b, uint32_t dims);

/** @return sum(|a[i]-b[i]|). */
double vector_distance_manhattan(const char *a, const char *b, uint32_t dims);
```

SQL layer (`sql/item_strfunc.h`):

```cpp
class Item_func_vector_distance final : public Item_real_func {
 public:
  Item_func_vector_distance(const POS &pos, Item *a, Item *b, Item *c);
  bool do_itemize(Parse_context *pc, Item **res) override;
  bool resolve_type(THD *thd) override;
  const char *func_name() const override;   // "distance"
  enum Functype functype() const override;  // VECTOR_DISTANCE_FUNC
  double val_real() override;
};
```

Other interface changes:

- `Item_func::Functype`: new value `VECTOR_DISTANCE_FUNC`.
- `Item_real_func`: new three-argument constructor.
- `share/messages_to_error_log.txt`: new message
  `ER_VECTOR_DISTANCE_SIMD_DISPATCH`.
- Debug keyword `vector_distance_force_scalar` in `init_server_components()`.

### 4.3 Implementation Steps

1. `vector-common/vector_distance.{h,cc}`: kernels.
   - Lane types per tier: `ScalarLanes` and `VectorLanes<N>` (N = 4, 8, 16).
   - `accumulate<Metric>()`: per-metric update of the lane accumulators.
     Manhattan clears the sign bit with an integer mask, since `std::fabs`
     does not accept vector types.
   - `reduce_lanes()`: converts the 16 lanes to double and adds them in the
     fixed tree, using whole-register additions while the stride covers a
     register.
   - `finish<Metric>()`: adds the `dims % 16` tail in double, and for
     `COSINE` computes `1 - ab / sqrt(|a|²·|b|²)`, returns +Inf for a zero
     denominator and clamps to [0, 2].
   - `lanes_kernel()` (float32 fast path) and `double_lanes_kernel()` (exact
     fallback); `needs_double<Metric>()` decides between them:
     - `EUCLIDEAN`, `DOT`: the sum is out of range.
     - `COSINE`: `ab` is non-finite or either norm sum is out of range
       (`ab` may legitimately be 0 for orthogonal vectors).
     - `MANHATTAN`: the sum is non-finite (no products, so subnormal values
       are exact).
   - `VECTOR_DISTANCE_KERNELS(tier, target, lanes)` instantiates the four
     kernels per tier. x86_64 tiers use `target("sse4.2")`, `target("avx2")`
     and `target("avx512f")`. NEON uses the baseline ISA. On `_WIN32` only
     the scalar tier is built.
   - `#pragma fp_contract(off)` for MSVC.
2. `sql/CMakeLists.txt`, `unittest/gunit/CMakeLists.txt`: add
   `vector_distance.cc`; compile it with `-ffp-contract=off` when the
   compiler is GCC or Clang (`MY_COMPILER_IS_GNU_OR_CLANG AND NOT
   WIN32_CLANG`). clang-cl builds only the scalar tier for baseline x86-64,
   which has no fused multiply-add instruction.
3. `sql/item_strfunc.{h,cc}`, `sql/item_func.h`, `sql/item_create.cc`:
   `Item_func_vector_distance` and registration of `DISTANCE` and
   `VECTOR_DISTANCE`.
4. `sql/mysqld.cc`: call `init_vector_distance_functions()` in
   `init_server_components()`, handle the debug keyword
   `vector_distance_force_scalar`, and log `ER_VECTOR_DISTANCE_SIMD_DISPATCH`
   after the error log components are set up.
5. `share/messages_to_error_log.txt`: error message.


### 4.4 Test Plan

MTR tests:

| Test | Covers |
| ---- | ------ |
| `main.distance_euclidean`, `main.distance_euclidean_squared`, `main.distance_manhattan`, `main.distance_cosine`, `main.distance_dot` (shared `include/distance.inc`) | FR1–FR11, FR13 per metric: arity, argument types, metric names and case, `NULL` handling, nullable metadata, dimension mismatch, oversized and malformed binary strings, unaligned arguments, known values, tiny and large values, block and tail paths, prepared statements, query contexts |
| `main.distance_cosine_range` | FR7: parallel and anti-parallel vectors stay in [0, 2] |
| `main.distance_nan_identical` | FR11: NaN/±Inf in identical vectors, in a 16-lane block and in the tail |
| `main.distance_reduction_order` | FR12: result does not depend on the reduction order |
| `main.distance_generated_columns` | FR12: `VECTOR` columns rejected in generated columns, functional indexes and `CHECK`; stored and virtual generated columns and functional indexes over binary-string columns for every metric; index range, ref and k-NN `ORDER BY ... LIMIT` access; `UPDATE`, `DELETE`, `ALTER TABLE ADD COLUMN/INDEX`, `CHECK TABLE`; errors raised while computing generated values |
| `main.distance_tier_portability` (debug) | FR12, FR15: fills a table on the best tier, restarts with `vector_distance_force_scalar` and back; checks stored values, index lookups with recomputed values, `UPDATE`/`DELETE` and `CHECK TABLE` on the other tier, and the error log text. The data (67 dimensions, magnitudes 1e-3 to 1e3) gives a different result on every row under another summation order. |
| `binlog.binlog_stm_unsafe_distance` | FR14: `INSERT`, `UPDATE` and `DELETE` with the function raise `ER_BINLOG_UNSAFE_STATEMENT` with `binlog_format=STATEMENT`; `SELECT` does not |
| `binlog.binlog_mix_unsafe_distance` | FR14: the same statements are logged as row events with `binlog_format=MIXED` |
| `perfschema.error_log` | FR15: filters the CPU-dependent startup message |

Unit tests (`unittest/gunit/vector_distance-t.cc`):

| Test group | Covers |
| ---------- | ------ |
| `VectorDistanceTest` | Known values, cosine zero-vector sentinel, unaligned vs. aligned input, parity with a double-precision reference (FR6, FR7, FR13) |
| `VectorDistanceTierParityTest` (every tier available on the host) | Bit-identical to scalar, extreme values, tiny values, non-finite identical vectors, cosine range (FR7, FR11, FR12) |
| `VectorDistanceDispatchTest` | Tier selection is idempotent, forced tier override, error log text fragment (FR15) |

Benchmark: `unittest/gunit/vector_distance_benchmark-t` (not run by default)
measures every metric, tier and size (4 to 16383 dimensions) for NFR3.

Bit-identical results checked across the scalar, SSE4.2, AVX2 and
AVX-512F tiers, and between GCC and Clang builds.

Tests not implemented in this change, which may be added later:

- NEON tier results compared with x86_64 results in CI on aarch64.
- A Windows (MSVC) build comparing results with a Linux build.
