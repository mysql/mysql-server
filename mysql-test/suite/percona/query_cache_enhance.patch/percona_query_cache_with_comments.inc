--source percona_query_cache_with_comments_clear.inc
let $query=/* with comment first */select * from t1;
eval $query;
--source percona_query_cache_with_comments_eval.inc

let $query=# with comment first
select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=-- with comment first
select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=/* with comment first and "quote" */select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=# with comment first and "quote"
select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=-- with comment first and "quote" 
select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=
    /* with comment and whitespaces first */select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query= 
    # with comment and whitespaces first
select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=
    -- with comment and whitespaces first
select * from t1;
--source percona_query_cache_with_comments_eval.inc

let $internal=* internal comment *;

let $query=select * /$internal/ from t1;
--source percona_query_cache_with_comments_eval.inc
let $query=select */$internal/ from t1;
--source percona_query_cache_with_comments_eval.inc
let $query=select */$internal/from t1;
--source percona_query_cache_with_comments_eval.inc

let $internal=* internal comment with "quote" *;

let $query=select * /$internal/ from t1;
--source percona_query_cache_with_comments_eval.inc
let $query=select */$internal/ from t1;
--source percona_query_cache_with_comments_eval.inc
let $query=select */$internal/from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1
;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1 ;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1	;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1
/* comment in the end */;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1
/* *\/ */;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1
/* comment in the end */
;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1 #comment in the end;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1 #comment in the end
;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1 -- comment in the end;
--source percona_query_cache_with_comments_eval.inc

let $query=select * from t1 -- comment in the end
;
--source percona_query_cache_with_comments_eval.inc

let $query=select */* a comment \*/from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select *# a comment \\
from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select *-- a comment \\
from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select "\\\\"" /* not a comment */" from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select "\\\\"" /*! not a comment */" from t1;
--source include/percona_query_cache_with_comments_eval.inc

# following two queries related to bug #856404.
# There are different queries, but opt_query_cache_strip_comments thinks that they are equal.
let $query=select ' \'  ' from t1;
--source percona_query_cache_with_comments_eval.inc

let $query=select ' \' /* comment inside quotes with internal backslash quote */' from t1;
--source percona_query_cache_with_comments_eval.inc
