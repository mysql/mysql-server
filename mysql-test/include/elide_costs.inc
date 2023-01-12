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

# Usage: --replace_regex $elide_time
# This converts:
# (cost=1.26..2.52 rows=2) (actual time=63.071..64.263 rows=5 loops=2)
# into:
# -> (cost=1.26..2.52 rows=2) (rows=5 loops=2)
#  (cost=0.35 rows=1)        -> (... rows=1)
let $elide_time = /[(]actual time=[0-9.e+-]+ rows/(actual rows/;
