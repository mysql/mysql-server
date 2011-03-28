#
# Run the built query in $sql, retry temporary errors
#

let $_continue = 1;
let $_retry = 0;
while($_continue)
{
  error 0,1297; # Allow OK or temporary error
  eval $sql;    # Run the query

  if (!$mysql_errno)
  {
    # Query suceeded, break the retry loop
    let $_continue = 0;
  }

  if ($mysql_errno)
  {
    # Query failed
    inc $_retry;
    echo Query failed with temporary error, retry:  $_retry;

    sleep 0.1;
  }
}
