###########################################
#
# This is a template of the include file define_engine.inc which 
# should be placed in storage/<engine>/mysql-test/storage_engine folder.
#
################################
#
# The name of the engine under test must be defined in $ENGINE variable.
# You can set it either here (uncomment and edit) or in your environment.
#
# let $ENGINE =;
#
################################
#
# The following three variables define specific options for columns and tables.
# Normally there should be none needed, but for some engines it can be different.
# If the engine requires specific column option for all or indexed columns,
# set them inside the comment, e.g. /*!NOT NULL*/.
# Do the same for table options if needed, e.g. /*!INSERT_METHOD=LAST*/

let $default_col_opts = /*!*/;
let $default_col_indexed_opts = /*!*/;
let $default_tbl_opts = /*!*/;

# INDEX, UNIQUE INDEX, PRIMARY KEY, special index type - choose the fist that the engine allows, 
# or set it to /*!*/ if none is supported

let $default_index = /*!INDEX*/;

# If the engine does not support the following types, replace them with the closest possible

let $default_int_type = INT(11);
let $default_char_type = CHAR(8);

################################

--disable_query_log
--disable_result_log

# Here you can place your custom MTR code which needs to be executed before each test,
# e.g. creation of an additional schema or table, etc.
# The cleanup part should be defined in cleanup_engine.inc

--enable_query_log
--enable_result_log
