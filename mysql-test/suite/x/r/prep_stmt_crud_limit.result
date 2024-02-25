
#
## Test direct execution of Crud messages with limit field
CREATE DATABASE xtest;

## I. Validation of Crud.Find limits
#
# 1. Different setting in old limit field
#   a. limit only to first row
#   b. limit only to first two rows
#   c. limit only to first hundred rows, still return all rows from collection
#   d. limit only to last two rows
#   e. limit only to last two rows, still the last one is out of resultset
# 2. Setting literals in limit_expr field
#   a. limit only to first row
#   b. limit only to first two rows
#   c. limit only to first hundred rows, still return all rows from collection
#   d. limit only to last two rows
#   e. limit only to last two rows, still the last one is out of resultset
# 3. Setting placeholders in limit_expr field
#   a. limit only to first row
#   b. limit only to first two rows
#   c. limit only to first hundred rows, still return all rows from collection
#   d. limit only to last two rows
#   e. limit only to last two rows, still the last one is out of resultset

## II. Validation of Crud.Delete limits
#
# 1. Different setting in old limit field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 2. Setting literals in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 3. Setting placeholders in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)

## III. Validation of Crud.Update limits
#
# 1. Different setting in old limit field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 2. Setting literals in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 3. Setting placeholders in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)

## IV. Validation invalid configuration of Crud limits
#
# 1. 'limit' and 'limit_expr' were set
#   a. in Crud.Find
#   b. in Crud.Update
#   d. in Crud.Delete
# 2. 'limit' uses 'offset' field when Crud doesn't support it
#   a. in Crud.Update
#   b. in Crud.Delete
# 2. 'limit_expr' uses 'offset' field when Crud doesn't support it
#   a. in Crud.Update
#   b. in Crud.Delete

#
## I.1

[Setup table coll]
[Assert Find operation and expect 1 rows returned, executed with limit {row_count:1}]
[Assert Find operation and expect 2 rows returned, executed with limit {row_count:2}]
[Assert Find operation and expect 9 rows returned, executed with limit {row_count:100}]
[Assert Find operation and expect 2 rows returned, executed with limit {row_count:2 offset: 7}]
[Assert Find operation and expect 1 rows returned, executed with limit {row_count:2 offset: 8}]

#
## I.2
[Assert Find operation and expect 1 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}}]
[Assert Find operation and expect 9 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}} offset{type:LITERAL literal {type:V_SINT v_signed_int:7}}}]
[Assert Find operation and expect 1 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}} offset{type:LITERAL literal {type:V_SINT v_signed_int:8}}}]

#
## I.3
[Assert Find operation and expect 1 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Find operation and expect 9 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0} offset{type:PLACEHOLDER position:1}}]
[Assert Find operation and expect 1 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0} offset { type:PLACEHOLDER position:1}}]

#
## II.1

[Setup table coll]
[Assert Delete operation and expect deletion of 1 rows, executed with limit {row_count:1}]
[Assert Delete operation and expect deletion of 2 rows, executed with limit {row_count:2}]
[Assert Delete operation and expect deletion of 6 rows, executed with limit {row_count:100}]

#
## II.2

[Setup table coll]
[Assert Delete operation and expect deletion of 1 rows, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
[Assert Delete operation and expect deletion of 2 rows, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}}]
[Assert Delete operation and expect deletion of 6 rows, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}}]

#
## II.3

[Setup table coll]
[Assert Delete operation and expect deletion of 1 rows, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Delete operation and expect deletion of 2 rows, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Delete operation and expect deletion of 6 rows, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]

#
## III.1

[Setup table coll]
[Assert Update operation and expect rows modified 1, executed with limit {row_count:1}]
[Assert Update operation and expect rows modified 2, executed with limit {row_count:2}]
[Assert Update operation and expect rows modified 9, executed with limit {row_count:100}]

#
## III.2
[Assert Update operation and expect rows modified 1, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
[Assert Update operation and expect rows modified 2, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}}]
[Assert Update operation and expect rows modified 9, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}}]

#
## III.3
[Assert Update operation and expect rows modified 1, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Update operation and expect rows modified 2, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Update operation and expect rows modified 9, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]

#
## IV.1
[Assert Find operation returns an error, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }]
Got expected error: Invalid message, one of 'limit' and 'limit_expr' fields is allowed. Received both (code 5000)
[Assert Update operation returns an error, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }]
Got expected error: Invalid message, one of 'limit' and 'limit_expr' fields is allowed. Received both (code 5000)
[Assert Delete operation returns an error, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }]
Got expected error: Invalid message, one of 'limit' and 'limit_expr' fields is allowed. Received both (code 5000)

#
## IV.2
[Assert Update operation returns an error, executed with limit { row_count:1  offset: 1 }]
Got expected error: Invalid parameter: non-zero offset value is not allowed for this operation (code 5012)
[Assert Delete operation returns an error, executed with limit { row_count:1  offset: 1 }]
Got expected error: Invalid parameter: non-zero offset value is not allowed for this operation (code 5012)

#
## IV.3
[Assert Update operation returns an error, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}} offset{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
Got expected error: Invalid parameter: offset value is not allowed for this operation (code 5012)
[Assert Delete operation returns an error, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}} offset{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
Got expected error: Invalid parameter: offset value is not allowed for this operation (code 5012)
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP DATABASE IF EXISTS xtest;

#
## Test prepared execution of Crud messages with limit field
CREATE DATABASE xtest;

## I. Validation of Crud.Find limits
#
# 1. Different setting in old limit field
#   a. limit only to first row
#   b. limit only to first two rows
#   c. limit only to first hundred rows, still return all rows from collection
#   d. limit only to last two rows
#   e. limit only to last two rows, still the last one is out of resultset
# 2. Setting literals in limit_expr field
#   a. limit only to first row
#   b. limit only to first two rows
#   c. limit only to first hundred rows, still return all rows from collection
#   d. limit only to last two rows
#   e. limit only to last two rows, still the last one is out of resultset
# 3. Setting placeholders in limit_expr field
#   a. limit only to first row
#   b. limit only to first two rows
#   c. limit only to first hundred rows, still return all rows from collection
#   d. limit only to last two rows
#   e. limit only to last two rows, still the last one is out of resultset

## II. Validation of Crud.Delete limits
#
# 1. Different setting in old limit field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 2. Setting literals in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 3. Setting placeholders in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)

## III. Validation of Crud.Update limits
#
# 1. Different setting in old limit field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 2. Setting literals in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)
# 3. Setting placeholders in limit_expr field
#   a. delete only first row
#   b. delete only first two rows
#   c. try to delete hundred rows, still only 6 delete (in collection)

## IV. Validation invalid configuration of Crud limits
#
# 1. 'limit' and 'limit_expr' were set
#   a. in Crud.Find
#   b. in Crud.Update
#   d. in Crud.Delete
# 2. 'limit' uses 'offset' field when Crud doesn't support it
#   a. in Crud.Update
#   b. in Crud.Delete
# 2. 'limit_expr' uses 'offset' field when Crud doesn't support it
#   a. in Crud.Update
#   b. in Crud.Delete

#
## I.1

[Setup table coll]
[Assert Find operation and expect 1 rows returned, executed with limit {row_count:1}]
[Assert Find operation and expect 2 rows returned, executed with limit {row_count:2}]
[Assert Find operation and expect 9 rows returned, executed with limit {row_count:100}]
[Assert Find operation and expect 2 rows returned, executed with limit {row_count:2 offset: 7}]
[Assert Find operation and expect 1 rows returned, executed with limit {row_count:2 offset: 8}]

#
## I.2
[Assert Find operation and expect 1 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}}]
[Assert Find operation and expect 9 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}} offset{type:LITERAL literal {type:V_SINT v_signed_int:7}}}]
[Assert Find operation and expect 1 rows returned, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}} offset{type:LITERAL literal {type:V_SINT v_signed_int:8}}}]

#
## I.3
[Assert Find operation and expect 1 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Find operation and expect 9 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Find operation and expect 2 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0} offset{type:PLACEHOLDER position:1}}]
[Assert Find operation and expect 1 rows returned, executed with limit_expr { row_count{ type:PLACEHOLDER position:0} offset { type:PLACEHOLDER position:1}}]

#
## II.1

[Setup table coll]
[Assert Delete operation and expect deletion of 1 rows, executed with limit {row_count:1}]
[Assert Delete operation and expect deletion of 2 rows, executed with limit {row_count:2}]
[Assert Delete operation and expect deletion of 6 rows, executed with limit {row_count:100}]

#
## II.2

[Setup table coll]
[Assert Delete operation and expect deletion of 1 rows, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
[Assert Delete operation and expect deletion of 2 rows, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}}]
[Assert Delete operation and expect deletion of 6 rows, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}}]

#
## II.3

[Setup table coll]
[Assert Delete operation and expect deletion of 1 rows, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Delete operation and expect deletion of 2 rows, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Delete operation and expect deletion of 6 rows, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]

#
## III.1

[Setup table coll]
[Assert Update operation and expect rows modified 1, executed with limit {row_count:1}]
[Assert Update operation and expect rows modified 2, executed with limit {row_count:2}]
[Assert Update operation and expect rows modified 9, executed with limit {row_count:100}]

#
## III.2
[Assert Update operation and expect rows modified 1, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
[Assert Update operation and expect rows modified 2, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:2}}}]
[Assert Update operation and expect rows modified 9, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:100}}}]

#
## III.3
[Assert Update operation and expect rows modified 1, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Update operation and expect rows modified 2, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]
[Assert Update operation and expect rows modified 9, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}}]

#
## IV.1
[Assert Find operation returns an error, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }]
Got expected error: Invalid message, one of 'limit' and 'limit_expr' fields is allowed. Received both (code 5000)
[Assert Update operation returns an error, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }]
Got expected error: Invalid message, one of 'limit' and 'limit_expr' fields is allowed. Received both (code 5000)
[Assert Delete operation returns an error, executed with limit_expr { row_count{ type:PLACEHOLDER position:0}} limit { row_count:1 }]
Got expected error: Invalid message, one of 'limit' and 'limit_expr' fields is allowed. Received both (code 5000)

#
## IV.2
[Assert Update operation returns an error, executed with limit { row_count:1  offset: 1 }]
Got expected error: Invalid parameter: non-zero offset value is not allowed for this operation (code 5012)
[Assert Delete operation returns an error, executed with limit { row_count:1  offset: 1 }]
Got expected error: Invalid parameter: non-zero offset value is not allowed for this operation (code 5012)

#
## IV.3
[Assert Update operation returns an error, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}} offset{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
Got expected error: Invalid parameter: offset value is not allowed for this operation (code 5012)
[Assert Delete operation returns an error, executed with limit_expr {row_count{type:LITERAL literal {type:V_SINT v_signed_int:1}} offset{type:LITERAL literal {type:V_SINT v_signed_int:1}}}]
Got expected error: Invalid parameter: offset value is not allowed for this operation (code 5012)
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP DATABASE IF EXISTS xtest;
