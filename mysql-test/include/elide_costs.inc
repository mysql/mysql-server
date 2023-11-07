# Usage: --replace_regex $elide_costs

#  (cost=1.26..2.52 rows=2)  -> (<cost elided> rows=2)
#  (cost=0.35 rows=1)        -> (... rows=1)
let $elide_costs = /[(]cost=[0-9.e+-]+ rows/(rows/;

# Usage: --replace_regex $elide_costs_and_time
# This converts:
# (cost=1.26..2.52 rows=2) (actual time=63.071..64.263 rows=5 loops=2)
# into:
# -> (rows=2) (actual rows=5 loops=2)
#  (cost=0.35 rows=1)        -> (... rows=1)
let $elide_costs_and_time = /[(]cost=[0-9.e+-]+ rows/(rows/ /[(]actual time=[0-9.e+-]+ rows/(actual rows/;

# Usage: --replace_regex $elide_costs_and_time_and_row_estimate
# This converts:
# (cost=1.26..2.52 rows=2) (actual time=63.071..64.263 rows=5 loops=2)
# into:
# -> (...) (actual rows=5 loops=2)
#  (cost=0.35 rows=1)        -> (... rows=1)
let $elide_costs_and_time_and_row_estimate = /[(]cost=[0-9.e+-]+ rows=[0-9.e+-]+[)]/(...)/ /[(]actual time=[0-9.e+-]+ rows/(actual rows/;

# Usage: --replace_regex $elide_time
# This converts:
# (cost=1.26..2.52 rows=2) (actual time=63.071..64.263 rows=5 loops=2)
# into:
# -> (cost=1.26..2.52 rows=2) (rows=5 loops=2)
#  (cost=0.35 rows=1)        -> (... rows=1)
let $elide_time = /[(]actual time=[0-9.e+-]+ rows/(actual rows/;

# Usage: --replace_regex $elide_costs_and_rows
#  (cost=1.26..2.52 rows=2)  -> (...)
#  (cost=0.35 rows=1)        -> (...)
let $elide_costs_and_rows = /[(]cost=[0-9.e+-]+ rows=[0-9.e+-]+[)]/(...)/;

# Usage: --replace_regex $elide_metrics
# This removes all metrics from 'EXPLAIN ANALYZE' or 'EXPLAIN FORMAT=TREE', meaning
# that it converts:
# (cost=1.26..2.52 rows=2) (actual time=63.071..64.263 rows=5 loops=2)
# into an empty string.
#  (cost=1.26..2.52 rows=2)  -> empty string
#  (cost=0.35 rows=1)        -> empty string
# Contrary to the other patterns above, it does not even leave an
# ellipsis for the elided metrics. This is to make the output the same
# regardless of whether there actually is some metric to elide.
let $elide_metrics = / *[(]cost=.*//;

# Usage: --replace_regex $elide_trace_costs_and_rows
# Removes metrics from optimizer trace (JSON)
#
# Filter out unstable cost and row estimates.  replace_regex
# allows only a *single* variable substitution
# (cf. do_get_replace_regex in mysqltest.cc), so build the
# entire expression incrementally.
let $elide_trace_costs_and_rows=/cost\": [0-9.]+/cost\": "elided"/;
let $elide_trace_costs_and_rows=$elide_trace_costs_and_rows /cost_for_plan\": [0-9.]+/cost_for_plan\": "elided"/;
let $elide_trace_costs_and_rows=$elide_trace_costs_and_rows /rows\": [0-9.]+/rows\": "elided"/;
let $elide_trace_costs_and_rows=$elide_trace_costs_and_rows /rows_for_plan\": [0-9.]+/rows_for_plan\": "elided"/;
let $elide_trace_costs_and_rows=$elide_trace_costs_and_rows /rows_to_scan\": [0-9.]+/rows_to_scan\": "elided"/;
let $elide_trace_costs_and_rows=$elide_trace_costs_and_rows /num_rows_estimate\": [0-9.]+/num_rows_estimate\": "elided"/;

# Usage: --replace_regex $elide_json_time
# Remove actual execution times from EXPLAIN ANALYZE FORMAT=JSON
#  "actual_first_row_ms": 0.328761 -> "actual_first_row_ms": "elided"
#  "actual_last_row_ms": 0.328761 -> "actual_last_row_ms": "elided"
let $elide_json_time=/row_ms": [0-9.]*/row_ms": "elided"/;

# Usage: --replace-regex $elide_json_costs
# This removes costs from EXPLAIN FORMAT=JSON for both optimizers.
# Original optimizer: "read_cost": "0.25"  -> read_cost: "elided"
# Hypergraph optimizer: "estimated_total_cost": 0.25  -> estimated_total_cost: "elided"
let $elide_json_costs=/cost": "?[0-9.]*"?/cost": "elided"/;

# Usage: --replace-regex $elide_json_costs_and_time
let $elide_json_costs_and_time=/cost": "?[0-9.]*"?/cost": "elided"/ /row_ms": [0-9.]*/row_ms": "elided"/;
