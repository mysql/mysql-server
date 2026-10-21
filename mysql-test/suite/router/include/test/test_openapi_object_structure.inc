
# The test uses HOST to have only paths in HREF fields.
--let $mrs_add_service_path="/svc"
--let $mrs_add_host_name=""
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/crud_operations
--source ../mrs/db_schema/add.inc
SET @schema_id_crud=@schema_id;

--let $mrs_add_db_object=view1
--let $mrs_add_db_object_path=/v1
--let $mrs_add_db_object_op=READ
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/t1
--let $mrs_add_db_object_op=READ
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/t2
--let $mrs_add_db_object_op=DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=table3
--let $mrs_add_db_object_path=/t3
--let $mrs_add_db_object_op=CREATE,READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=table4
--let $mrs_add_db_object_path=/t4
--let $mrs_add_db_object_op=UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/different_types
--source ../mrs/db_schema/add.inc
SET @schema_id_different_types=@schema_id;

# all_types is metadata-only so its OpenAPI schema has no properties.
--let $mrs_data_provider_allow_missing_object=1
--let $mrs_add_db_object=all_types
--let $mrs_add_db_object_path=/all_types
--let $mrs_add_db_object_op=CREATE,READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc
SET @db_object_id_all_types=@db_object_id;

--let $mrs_add_schema=pk_types_schema
--let $mrs_add_schema_path=/different_types_as_pk
--source ../mrs/db_schema/add.inc
SET @schema_id_different_types_as_pk=@schema_id;

--let $mrs_add_db_object=t_int
--let $mrs_add_db_object_path=/t_int
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_float
--let $mrs_add_db_object_path=/t_float
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_bin
--let $mrs_add_db_object_path=/t_bin
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_bit
--let $mrs_add_db_object_path=/t_bit
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_enum
--let $mrs_add_db_object_path=/t_enum
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_set
--let $mrs_add_db_object_path=/t_set
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_timestamp
--let $mrs_add_db_object_path=/t_timestamp
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_date
--let $mrs_add_db_object_path=/t_date
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

--let $mrs_add_db_object=t_decimal
--let $mrs_add_db_object_path=/t_decimal
--let $mrs_add_db_object_op=READ,UPDATE,DELETE
--source ../mrs/db_object/add.inc

if ($mrs_schema_version>2)
{
  --disable_query_log
  --disable_result_log
  UPDATE `mysql_rest_service_metadata`.`service` SET name="" WHERE url_context_root="/svc";
  --enable_query_log
  --enable_result_log
}
