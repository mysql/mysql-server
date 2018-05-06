#
# This include file checks that the function statement_digest() and
# performance_schema agree on what the digest is for the query in
# $check_digest_text_query.

let $compare_digests_fn=statement_digest_text;
let $compare_digests_query=$check_digest_text_query;
let $compare_digests_pfs_query=$check_digest_text_query;
let $compare_digests_pfs_column=digest_text;
source compare_digests.inc;
