# Prerequisites:
# connections:
#   cq - connection where queries are run
#   ce - connection where explain is run
# variables:
#   query - query to explain
#   format - explain format: 'json' for json, or traditional otherwise
#   point  - syncing point
#   replace - a string that will be used by replace_regex to mask instabilities

--disable_query_log
--enable_result_log

if ($format == 'json')
{
  let $fmt= FORMAT=JSON;
}

if ($format != 'json')
{
  let $fmt= FORMAT=TRADITIONAL;
}

connection cq;
SET DEBUG_SYNC= 'RESET';

# Grab output of regular explain
--replace_regex $replace
--output $MYSQL_TMP_DIR/EN
--skip_if_hypergraph  # Depends on the query plan.
eval EXPLAIN $fmt $query;


# Grap output of EXPLAIN FOR CONNECTION
let $QID= `SELECT CONNECTION_ID()`;
eval SET DEBUG_SYNC= '$point SIGNAL ready_for_explain WAIT_FOR explained';
send_eval $query;

connection ce;
# Let the query reach sync point (i.e pass optimization and prepare plan)

SET DEBUG_SYNC= 'now WAIT_FOR ready_for_explain';
SET DEBUG_SYNC= 'after_explain_other SIGNAL explained';
--replace_regex $replace
--output $MYSQL_TMP_DIR/EO
--skip_if_hypergraph  # Depends on the query plan.
eval EXPLAIN $fmt FOR CONNECTION $QID;

connection cq;
--disable_result_log
reap;
--enable_result_log
SET DEBUG_SYNC= 'RESET';

--error 0,1,2
--exec diff -U 1 $MYSQL_TMP_DIR/EN $MYSQL_TMP_DIR/EO > $MYSQL_TMP_DIR/ED;
let $status= $__error;
if ($status == 2)
{
  die "Error from diff";
}

if ($status)
{
echo QUERY: $query;
echo STATUS:NOT_EQUAL;
echo SYNC_POINT:$point;
echo Diff: EXPLAIN NORMAL vs OTHER;
--exec grep -v -e "---" -e "+++" $MYSQL_TMP_DIR/ED;
}

if (!$status)
{
echo QUERY: $query;
echo STATUS: EQUAL;
}

remove_file $MYSQL_TMP_DIR/EN;
remove_file $MYSQL_TMP_DIR/EO;
remove_file $MYSQL_TMP_DIR/ED;

--disable_result_log
