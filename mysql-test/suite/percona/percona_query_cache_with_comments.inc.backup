--source include/percona_query_cache_with_comments_clear.inc
let $query=/* with comment first */select * from t1;
eval $query;
--source include/percona_query_cache_with_comments_eval.inc

let $query=# with comment first
select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=-- with comment first
select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=/* with comment first and "quote" */select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=# with comment first and "quote"
select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=-- with comment first and "quote" 
select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=
    /* with comment and whitespaces first */select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query= 
    # with comment and whitespaces first
select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=
    -- with comment and whitespaces first
select * from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $internal=* internal comment *;

let $query=select * /$internal/ from t1;
--source include/percona_query_cache_with_comments_eval.inc
let $query=select */$internal/ from t1;
--source include/percona_query_cache_with_comments_eval.inc
let $query=select */$internal/from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $internal=* internal comment with "quote" *;

let $query=select * /$internal/ from t1;
--source include/percona_query_cache_with_comments_eval.inc
let $query=select */$internal/ from t1;
--source include/percona_query_cache_with_comments_eval.inc
let $query=select */$internal/from t1;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1
;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1 ;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1	;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1
/* comment in the end */;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1
/* comment in the end */
;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1 #comment in the end;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1 #comment in the end
;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1 -- comment in the end;
--source include/percona_query_cache_with_comments_eval.inc

let $query=select * from t1 -- comment in the end
;
--source include/percona_query_cache_with_comments_eval.inc
