###########################################
#
# This include file executes define_engine.inc which belongs to the storage engine
# and then checks and sets test options and parameters.
#
# The name of the engine under test must be defined in $ENGINE variable.
# You can define it either in define_engine.inc or in your environment.
#
################################
#
# The following three variables define specific options for columns and tables.
#   Normally there should be none needed, but for some engines it can be different.
# If the engine requires specific column option for all or indexed columns,
#   set them inside the comment, e.g. /*!NOT NULL*/.
#   Do the same for table options if needed, e.g. /*!INSERT_METHOD=LAST*/

let $default_col_opts = /*!*/;
let $default_col_indexed_opts = /*!*/;
let $default_tbl_opts = /*!*/;

# INDEX, UNIQUE INDEX, PRIMARY KEY, special index type -- choose minimal that the engine allows, 
# or set it to /*!*/ if none is supported

let $default_index = /*!INDEX*/;

# If the engine does not support these types, replace them with the closest possible

let $default_int_type = INT(11);
let $default_char_type = CHAR(8);

################################
#
# Here the actual work starts
# define_engine.inc will set the ENGINE variable 
#   and possibly redefine some of default_* variables

--source define_engine.inc

if (!$ENGINE)
{
  --skip Storage engine under test is not defined, export ENGINE env variable or set it in define_engine.inc
}

# Check that the storage engine is loaded. Here we don't need to worry about the case,
# as I_S is case-insensitive.

if (!`SELECT engine FROM INFORMATION_SCHEMA.ENGINES WHERE engine = '$ENGINE' AND support IN ('YES', 'DEFAULT', 'ENABLED')`)
{
  --skip The test requires $ENGINE engine
}

# Further in tests we will use $storage_engine variable

let $storage_engine = $ENGINE;

# In case somebody removed comment tags in define_engine.inc
if (!$default_col_opts)
{
  let $default_col_opts = /*!*/;
}
if (!$default_col_indexed_opts)
{
  let $default_col_indexed_opts = /*!*/;
}
if (!$default_tbl_opts)
{
  let $default_tbl_opts = /*!*/;
}
if (!$default_index)
{
  let $default_index = /*!*/;
}

# Adding a comment after default options helps later to mask the custom values in result output
let $default_index = $default_index /*Custom index*/;
let $default_col_opts = $default_col_opts /*Custom column options*/;
let $default_col_indexed_opts = $default_col_indexed_opts /*Custom indexed column options*/;
let $default_tbl_opts = $default_tbl_opts /*Custom table options*/;


# Finally, set some variables which will make our life easier later.
# For many tests, we need a simple INT or CHAR column, but with
# (optional) column attributes which the engine might require.
# Also, a test might be called by another test, and the calling test
# might need additional attributes. So, here we are collecting them all together.
# Example:
# CSV engine requires all columns to be NOT NULL, so its define_engine.inc file will contain
# let $default_col_opts = /*!NOT NULL*/;
# Then, we have test1.test which calls have_engine.inc.
# If it's executed directly, it will have $int_col = 'INT(11) NOT NULL' for CSV and 'INT(11)' for MyISAM.
# Another test, test2.test might set $extra_type_opts = UNSIGNED and/or $extra_col_opts = NULL 
# and call test1.test.
# If both are set, test1.test will be executed for MyISAM with 'INT(11) UNSIGNED NULL',
# and for CSV INT(11) UNSIGNED NOT NULL NULL (and will fail because CSV does not support nullable columns)

let $col_opts = $default_col_opts;
let $col_indexed_opts = $default_col_indexed_opts;

if ($extra_col_opts)
{
  let $col_opts = $col_opts $extra_col_opts;
  let $col_indexed_opts = $col_indexed_opts $extra_col_opts;
}

if ($extra_type_opts)
{
  let $col_opts = $extra_type_opts $col_opts;
  let $col_indexed_opts = $extra_type_opts $col_indexed_opts;
}

let $int_col = $default_int_type $col_opts;
let $int_indexed_col = $default_int_type $col_indexed_opts;

let $char_col = $default_char_type $col_opts;
let $char_indexed_col = $default_char_type $col_indexed_opts;



if (`SELECT @@global.log_bin = 'ON' AND @@global.binlog_format = 'STATEMENT'`)
{
  --disable_query_log
  CALL mtr.add_suppression('Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT');
  --enable_query_log
}

--disable_abort_on_error

