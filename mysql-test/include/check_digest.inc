#
# This include file checks that the function statement_digest() and
# performance_schema agree on what the digest is for the query in
# $check_digest_statement.
#

let $compare_digests_fn=statement_digest;
let $compare_digests_query=$check_digest_statement;
let $compare_digests_pfs_query=$check_digest_statement;
let $compare_digests_pfs_column=digest;
source compare_digests.inc;
