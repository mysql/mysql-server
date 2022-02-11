# Usage: --replace_regex $elide_costs

#  (cost=1.26..2.52 rows=2)  -> (<cost elided> rows=2)
#  (cost=0.35 rows=1)        -> (... rows=1)
let $elide_costs = /[(]cost=[0-9.]+ rows/(rows/;
