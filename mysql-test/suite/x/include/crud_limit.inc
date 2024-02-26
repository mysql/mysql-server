## Usage
#
# --let $crud_limit_macros=PATH
# --source ../include/crud_limit.inc
#

--write_file $MYSQL_TMP_DIR/mysqlx-in.tmp

-->import crud.macro
-->import crud_insert.macro
-->import %TEST_MACROS%

-->echo
-->echo ## I. Validation of Crud.Find limits
-->echo #
-->echo # 1. Different setting in old limit field
-->echo #   a. limit only to first row
-->echo #   b. limit only to first two rows
-->echo #   c. limit only to first hundred rows, still return all rows from collection
-->echo #   d. limit only to last two rows
-->echo #   e. limit only to last two rows, still the last one is out of resultset
-->echo # 2. Setting literals in limit_expr field
-->echo #   a. limit only to first row
-->echo #   b. limit only to first two rows
-->echo #   c. limit only to first hundred rows, still return all rows from collection
-->echo #   d. limit only to last two rows
-->echo #   e. limit only to last two rows, still the last one is out of resultset
-->echo # 3. Setting placeholders in limit_expr field
-->echo #   a. limit only to first row
-->echo #   b. limit only to first two rows
-->echo #   c. limit only to first hundred rows, still return all rows from collection
-->echo #   d. limit only to last two rows
-->echo #   e. limit only to last two rows, still the last one is out of resultset
-->echo
-->echo ## II. Validation of Crud.Delete limits
-->echo #
-->echo # 1. Different setting in old limit field
-->echo #   a. delete only first row
-->echo #   b. delete only first two rows
-->echo #   c. try to delete hundred rows, still only 6 delete (in collection)
-->echo # 2. Setting literals in limit_expr field
-->echo #   a. delete only first row
-->echo #   b. delete only first two rows
-->echo #   c. try to delete hundred rows, still only 6 delete (in collection)
-->echo # 3. Setting placeholders in limit_expr field
-->echo #   a. delete only first row
-->echo #   b. delete only first two rows
-->echo #   c. try to delete hundred rows, still only 6 delete (in collection)
-->echo
-->echo ## III. Validation of Crud.Update limits
-->echo #
-->echo # 1. Different setting in old limit field
-->echo #   a. delete only first row
-->echo #   b. delete only first two rows
-->echo #   c. try to delete hundred rows, still only 6 delete (in collection)
-->echo # 2. Setting literals in limit_expr field
-->echo #   a. delete only first row
-->echo #   b. delete only first two rows
-->echo #   c. try to delete hundred rows, still only 6 delete (in collection)
-->echo # 3. Setting placeholders in limit_expr field
-->echo #   a. delete only first row
-->echo #   b. delete only first two rows
-->echo #   c. try to delete hundred rows, still only 6 delete (in collection)
-->echo
-->echo ## IV. Validation invalid configuration of Crud limits
-->echo #
-->echo # 1. 'limit' and 'limit_expr' were set
-->echo #   a. in Crud.Find
-->echo #   b. in Crud.Update
-->echo #   d. in Crud.Delete
-->echo # 2. 'limit' uses 'offset' field when Crud doesn't support it
-->echo #   a. in Crud.Update
-->echo #   b. in Crud.Delete
-->echo # 2. 'limit_expr' uses 'offset' field when Crud doesn't support it
-->echo #   a. in Crud.Update
-->echo #   b. in Crud.Delete

-->callmacro Setup_testcase

-->echo
-->echo #
-->echo ## I.1
-->callmacro Setup_collection_with_data
-->callmacro Find_execute_and_expect	1	limit {row_count:1}
-->callmacro Find_execute_and_expect	2	limit {row_count:2}
-->callmacro Find_execute_and_expect	9	limit {row_count:100}
-->callmacro Find_execute_and_expect	2	limit {row_count:2 offset: 7}
-->callmacro Find_execute_and_expect	1	limit {row_count:2 offset: 8}

-->echo
-->echo #
-->echo ## I.2
callmacro Find_execute_and_expect	1
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}};
callmacro Find_execute_and_expect	2
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}};
callmacro Find_execute_and_expect	9
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}};
callmacro Find_execute_and_expect	2
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}} offset{type:LITERAL literal {type:V_SINT v_signed_int:7}}};
callmacro Find_execute_and_expect	1
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}} offset{type:LITERAL literal {type:V_SINT v_signed_int:8}}};

-->echo
-->echo #
-->echo ## I.3
callmacro Find_execute_and_expect	1
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 1 %ANY_END% };
callmacro Find_execute_and_expect	2
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 2 %ANY_END% };
callmacro Find_execute_and_expect	9
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 100 %ANY_END% };
callmacro Find_execute_and_expect	2
	limit_expr { row_count{ type:PLACEHOLDER position:0} offset{type:PLACEHOLDER position:1}}
	args{ %ANY_START% type:V_SINT v_signed_int: 2 %ANY_END% } args{ %ANY_START% type:V_SINT v_signed_int: 7 %ANY_END% };
callmacro Find_execute_and_expect	1
	limit_expr { row_count{ type:PLACEHOLDER position:0} offset { type:PLACEHOLDER position:1}}
	args{ %ANY_START% type:V_SINT v_signed_int: 2 %ANY_END% } args{ %ANY_START% type:V_SINT v_signed_int: 8 %ANY_END% };

-->echo
-->echo #
-->echo ## II.1
-->callmacro Setup_collection_with_data
-->callmacro Delete_execute_and_expect	1	limit {row_count:1}
-->callmacro Delete_execute_and_expect	2	limit {row_count:2}
-->callmacro Delete_execute_and_expect	6	limit {row_count:100}

-->echo
-->echo #
-->echo ## II.2
-->callmacro Setup_collection_with_data
callmacro Delete_execute_and_expect	1
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}};
callmacro Delete_execute_and_expect	2
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}};
callmacro Delete_execute_and_expect	6
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}};

-->echo
-->echo #
-->echo ## II.3
-->callmacro Setup_collection_with_data
callmacro Delete_execute_and_expect	1
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 1 %ANY_END% };
callmacro Delete_execute_and_expect	2
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 2 %ANY_END% };
callmacro Delete_execute_and_expect	6
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 100 %ANY_END% };

-->echo
-->echo #
-->echo ## III.1
-->callmacro Setup_collection_with_data
-->callmacro Update_execute_and_expect	1	limit {row_count:1}
-->callmacro Update_execute_and_expect	2	limit {row_count:2}
-->callmacro Update_execute_and_expect	9	limit {row_count:100}

-->echo
-->echo #
-->echo ## III.2
callmacro Update_execute_and_expect	1
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}};
callmacro Update_execute_and_expect	2
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}};
callmacro Update_execute_and_expect	9
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}};

-->echo
-->echo #
-->echo ## III.3
callmacro Update_execute_and_expect	1
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 1 %ANY_END% };
callmacro Update_execute_and_expect	2
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 2 %ANY_END% };
callmacro Update_execute_and_expect	9
	limit_expr { row_count{ type:PLACEHOLDER position:0}}
	args{ %ANY_START% type:V_SINT v_signed_int: 100 %ANY_END% };

-->echo
-->echo #
-->echo ## IV.1
callmacro Find_execute_and_expect_error	ER_X_BAD_MESSAGE
	limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }
	args{ %ANY_START% type:V_SINT v_signed_int: 1 %ANY_END% };

callmacro Update_execute_and_expect_error	ER_X_BAD_MESSAGE
	limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }
	args{ %ANY_START% type:V_SINT v_signed_int: 1 %ANY_END% };

callmacro Delete_execute_and_expect_error	ER_X_BAD_MESSAGE
	limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }
	args{ %ANY_START% type:V_SINT v_signed_int: 1 %ANY_END%};

-->echo
-->echo #
-->echo ## IV.2
callmacro Update_execute_and_expect_error	ER_X_INVALID_ARGUMENT
	limit { row_count:1  offset: 1 };

callmacro Delete_execute_and_expect_error	ER_X_INVALID_ARGUMENT
	limit { row_count:1  offset: 1 };

-->echo
-->echo #
-->echo ## IV.3
callmacro Update_execute_and_expect_error	ER_X_INVALID_ARGUMENT
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}} offset{type:LITERAL literal {type:V_SINT v_signed_int:1}}};

callmacro Delete_execute_and_expect_error	ER_X_INVALID_ARGUMENT
	limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}} offset{type:LITERAL literal {type:V_SINT v_signed_int:1}}};

EOF

CREATE DATABASE xtest;

exec $MYSQLXTEST
 -u x_root
 --password=''
 --schema=xtest
 -v%TEST_MACROS%=$crud_limit_macros
 --file=$MYSQL_TMP_DIR/mysqlx-in.tmp 2>&1;

## Cleanup
DROP DATABASE IF EXISTS xtest;

--remove_file $MYSQL_TMP_DIR/mysqlx-in.tmp
