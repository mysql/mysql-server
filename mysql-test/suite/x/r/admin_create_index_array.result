CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';

Create collection 'data'
========================

command ok

Insert documents into collection 'data'
=======================================

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)
## 1. Create and drop array index ($.path)

Create array index 'integer_idx' on field 'integer'
===================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'integer_idx'" returns in "Key_name" column, following value integer_idx]

Drop array index 'integer_idx'
==============================

command ok

Create array index 'text_idx' on field 'text'
=============================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'text_idx'" returns in "Key_name" column, following value text_idx]

Drop array index 'text_idx'
===========================

command ok

Create array index 'text_default_type_idx' on field 'text'
==========================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'text_default_type_idx'" returns in "Key_name" column, following value text_default_type_idx]

Drop array index 'text_default_type_idx'
========================================

command ok

Create array index 'time_idx' on field 'time'
=============================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'time_idx'" returns in "Key_name" column, following value time_idx]

Drop array index 'time_idx'
===========================

command ok

Create array index 'date_idx' on field 'date'
=============================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'date_idx'" returns in "Key_name" column, following value date_idx]

Drop array index 'date_idx'
===========================

command ok

Create array index 'datetime_idx' on field 'datetime'
=====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'datetime_idx'" returns in "Key_name" column, following value datetime_idx]

Drop array index 'datetime_idx'
===============================

command ok

Create array index 'float_idx' on field 'float'
===============================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'float_idx'" returns in "Key_name" column, following value float_idx]

Drop array index 'float_idx'
============================

command ok
## 1. Create and drop array index ($.path[*])

Create array index 'integer_arr_idx' on field 'integer[*]'
==========================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'integer_arr_idx'" returns in "Key_name" column, following value integer_arr_idx]

Drop array index 'integer_arr_idx'
==================================

command ok

Create array index 'text_arr_idx' on field 'text[*]'
====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'text_arr_idx'" returns in "Key_name" column, following value text_arr_idx]

Drop array index 'text_arr_idx'
===============================

command ok

Create array index 'text_arr_default_type_idx' on field 'text[*]'
=================================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'text_arr_default_type_idx'" returns in "Key_name" column, following value text_arr_default_type_idx]

Drop array index 'text_arr_default_type_idx'
============================================

command ok

Create array index 'time_arr_idx' on field 'time[*]'
====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'time_arr_idx'" returns in "Key_name" column, following value time_arr_idx]

Drop array index 'time_arr_idx'
===============================

command ok

Create array index 'date_arr_idx' on field 'date[*]'
====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'date_arr_idx'" returns in "Key_name" column, following value date_arr_idx]

Drop array index 'date_arr_idx'
===============================

command ok

Create array index 'datetime_arr_idx' on field 'datetime[*]'
============================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'datetime_arr_idx'" returns in "Key_name" column, following value datetime_arr_idx]

Drop array index 'datetime_arr_idx'
===================================

command ok

Create array index 'float_arr_idx' on field 'float[*]'
======================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'float_arr_idx'" returns in "Key_name" column, following value float_arr_idx]

Drop array index 'float_arr_idx'
================================

command ok

Create array index 'object_idx' on field 'object[*].number'
===========================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'object_idx'" returns in "Key_name" column, following value object_idx]

Drop array index 'object_idx'
=============================

command ok
## 2. Try to create 'multi-field' array index (array-array)
Got expected error: This version of MySQL doesn't yet support 'more than one multi-valued key part per index' (code 1235)
## 2. Create and drop 'multi-field' array index (array-scalar)

command ok
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'scalar_text_idx'" returns in "Key_name" column, following value scalar_text_idx]

Drop array index 'scalar_text_idx'
==================================

command ok
## 3. Create and drop 'unique' array index

command ok
Verify ["SHOW index FROM xtest.data WHERE Key_name = 'unique_idx'" returns in "Key_name" column, following value unique_idx]

Drop array index 'unique_idx'
=============================

command ok
Got expected error: Duplicate entry '[3.' for key 'data.fail_idx' (code 1062)
## 3. Create 'required' array index
Got expected error: Unsupported argument specification for '$.integer' (code 5017)
## 4. Verify usage array index
## 4.1 with JSON_CONTAINS

command ok
Verify ["EXPLAIN SELECT * FROM xtest.data USE INDEX(`integer_idx`) WHERE json_contains(doc->'$.integer', cast('[11]' as json))" returns in "possible_keys" column, following value integer_idx]
## 4. Verify usage array index
## 4.2 with JSON_OVERLAPS
Verify ["EXPLAIN SELECT * FROM xtest.data USE INDEX(`integer_idx`) WHERE json_overlaps(doc->'$.integer', cast('[11]' as json))" returns in "possible_keys" column, following value integer_idx]
## 5. null value is not supported in indexed arrays

Create collection 'data1'
=========================

command ok

1 rows affected
Got expected error (code 0)
Got expected error: Invalid JSON value for CAST for functional index 'integer_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'text_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'text_default_type_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'time_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'date_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'datetime_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'float_idx'. (code 3903)

Create collection 'data2'
=========================

command ok

1 rows affected
Got expected error (code 0)
Got expected error: Invalid JSON value for CAST for functional index 'integer_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'text_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'text_default_type_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'time_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'date_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'datetime_idx'. (code 3903)
Got expected error: Invalid JSON value for CAST for functional index 'float_idx'. (code 3903)
## 6. Create different types of index on an empty array

Create collection 'data3'
=========================

command ok

1 rows affected
Got expected error (code 0)

Create array index 'empty_signed_idx' on field 'empty'
======================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_signed_idx'" returns in "Key_name" column, following value empty_signed_idx]

Drop array index 'empty_signed_idx'
===================================

command ok

Create array index 'empty_unsigned_idx' on field 'empty'
========================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_unsigned_idx'" returns in "Key_name" column, following value empty_unsigned_idx]

Drop array index 'empty_unsigned_idx'
=====================================

command ok

Create array index 'empty_char_idx' on field 'empty'
====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_char_idx'" returns in "Key_name" column, following value empty_char_idx]

Drop array index 'empty_char_idx'
=================================

command ok

Create array index 'empty_date_idx' on field 'empty'
====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_date_idx'" returns in "Key_name" column, following value empty_date_idx]

Drop array index 'empty_date_idx'
=================================

command ok

Create array index 'empty_time_idx' on field 'empty'
====================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_time_idx'" returns in "Key_name" column, following value empty_time_idx]

Drop array index 'empty_time_idx'
=================================

command ok

Create array index 'empty_datetime_idx' on field 'empty'
========================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_datetime_idx'" returns in "Key_name" column, following value empty_datetime_idx]

Drop array index 'empty_datetime_idx'
=====================================

command ok

Create array index 'empty_decimal_idx' on field 'empty'
=======================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_decimal_idx'" returns in "Key_name" column, following value empty_decimal_idx]

Drop array index 'empty_decimal_idx'
====================================

command ok

Create array index 'empty_binary_idx' on field 'empty'
======================================================

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data3 WHERE Key_name = 'empty_binary_idx'" returns in "Key_name" column, following value empty_binary_idx]

Drop array index 'empty_binary_idx'
===================================

command ok
## 7. Create index on a field having mixed data types

Create collection 'data4'
=========================

command ok

1 rows affected
Got expected error (code 0)
Got expected error: Invalid JSON value for CAST for functional index 'mixed_signed_idx'. (code 3903)
Got expected error: Value is out of range for functional index 'mixed_unsigned_idx' at row 1 (code 3752)

command ok
Got expected error (code 0)
Got expected error: Data truncated for functional index 'mixed_date_idx' at row 1 (code 3751)
Got expected error: Data truncated for functional index 'mixed_time_idx' at row 1 (code 3751)
Got expected error: Data truncated for functional index 'mixed_datetime_idx' at row 1 (code 3751)
Got expected error: Invalid JSON value for CAST for functional index 'mixed_decimal_idx'. (code 3903)

command ok
Got expected error (code 0)
## 8. Create index on a field and insert document with that field missing

Create collection 'data5'
=========================

command ok

1 rows affected
Got expected error (code 0)

command ok
Got expected error (code 0)
Verify ["SHOW index FROM xtest.data5 WHERE Key_name = 'emp_id_idx'" returns in "Key_name" column, following value emp_id_idx]

1 rows affected
Got expected error (code 0)
## 9. Try to create index with type as JSON/TEXT[(N)] [charset_info]/CHAR[(N)] [charset_info]

Create collection 'data6'
=========================

command ok
Got expected error: Invalid or unsupported type specification for array index 'JSON' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'text' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'text(64)' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'text(64) charset latin1 collate latin1_bin' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'text(64) charset latin1' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'text(64) collate latin1_bin' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'char(64) charset latin1' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'char(64) collate latin1_bin' (code 5017)
Got expected error: Invalid or unsupported type specification for array index 'char(64) charset latin1 collate latin1_bin' (code 5017)
## 10. Increasing admin commands counters

Create collection 'data8'
=========================

command ok
Verify [Status variable "Mysqlx_stmt_create_collection_index" needs to have a value of 61]

command ok
Got expected error (code 0)
Verify [Status variable "Mysqlx_stmt_create_collection_index" needs to have a value of 62]
Verify [Status variable "Mysqlx_stmt_drop_collection_index" needs to have a value of 25]

command ok
Verify [Status variable "Mysqlx_stmt_drop_collection_index" needs to have a value of 26]
Mysqlx.Ok {
  msg: "bye!"
}
ok
