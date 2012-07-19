#
# NULL attribute and DEFAULT NULL in columns
#
# Usage:
# let $col_definition = <column type (and possibly more options)>;
# let $col_default = <default non-null value for a column>;
# --source col_null.inc
#
# We will add NULL at the end of $col;
#
# Also, if $col_default is defined, 
# we will create a table with 3 columns (one with DEFAULT NULL, 
# one with DEFAULT $col_default, and one without any default)
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--let $create_definition = c $col_definition NULL
--source create_table.inc
SHOW COLUMNS IN t1;
if ($mysql_errname)
{
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  INSERT INTO t1 (c) VALUES (NULL);
  if ($mysql_errname)
  {
    --let $functionality = NULLable columns
    --source unexpected_result.inc
  }
  SELECT COUNT(c), COUNT(*) FROM t1;

  DROP TABLE t1;
}

if ($col_default != '')
{
  let $create_definition =
    c $col_definition NULL,
    c1 $col_definition NULL DEFAULT NULL,
    c2 $col_definition NULL DEFAULT $col_default
  ;
  --source create_table.inc
  if ($mysql_errname)
  {
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    SHOW COLUMNS IN t1;

    INSERT INTO t1 (c) VALUES (NULL);
    SELECT COUNT(c2), COUNT(c1), COUNT(c), COUNT(*) FROM t1;

    DROP TABLE t1;
  }
}

# We don't want to preserve it
let $col_default = ;

