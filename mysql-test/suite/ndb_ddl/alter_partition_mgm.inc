if (!$query)
{
  die Need the query to test with provided in $query;
}


#
# Check the current limitation in MySQL that you can't specify
# ALGORITHM=[DEFAULT|COPY|INPLACE] with a comma afterwards for certain
# partition management commands. Verify that this claim is
# true for the current query and such an ALTER fails with syntax error.
#
let $syntax_error_query =
   `select REPLACE('$query', 't1', 't1 ALGORITHM=INPLACE,')`;
#echo $syntax_error_query;

--error ER_PARSE_ERROR
eval $syntax_error_query;


#
# Check that you can specify ALGORITHM=INPLACE
# without comma even if INPLACE is not supported
#
let $inplace_query =
   `select REPLACE('$query', 't1', 't1 ALGORITHM=INPLACE')`;
#echo $inplace_query;

--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
eval $inplace_query;


#
# Check that you can specify ALGORITHM=COPY
# and that copying alter is suported
#
let $copying_query =
   `select REPLACE('$query', 't1', 't1 ALGORITHM=COPY')`;
#echo $copying_query;

eval $copying_query;


#
# Check that the query performs copying ALTER
# when not specifying any ALGORITHM=
#
eval $query;

# Reset parameters which should change between each invocation
let $query =;

