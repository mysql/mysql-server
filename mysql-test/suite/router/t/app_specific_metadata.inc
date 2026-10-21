--source include/have_router.inc

--let $extra_mrs_router_id=1
--source ../include/predefined_setup/configure_router_mrs_root.inc
--source ../include/schema/basic_schema.sql
--source ../include/schema/functions_schema.sql

CREATE USER user_valid@'%' IDENTIFIED BY 'pwd';

--source ../include/mrs/start_object_definition.inc

--echo Create a service with NO metadata defined on the service level
--let $mrs_add_service_path="/svc_no_md"
--let $mrs_add_host_name=""
--source ../include/mrs/service/add.inc

--echo Create a service with metadata defined on the service level
--let $mrs_add_service_path="/svc_md"
--let $mrs_add_host_name=""
--let $mrs_add_service_metadata='{"app_specific_md":"service level"}'
--source ../include/mrs/service/add.inc

--echo Create a schema endpoint with NO metadata defined on the schema level
--let $mrs_add_schema=func_schema
--let $mrs_add_schema_path=/func_schema_no_md
--source ../include/mrs/db_schema/add.inc

--echo # Create a function endpoint with NO metadata defined on the object level
--let $mrs_add_db_object=func_sum
--let $mrs_add_db_object_path=/func_sum_no_md
--let $mrs_add_db_object_type=FUNCTION
--let $mrs_add_db_object_format=ITEM
--source ../include/mrs/db_object/add.inc

--echo # Create a function endpoint with metadata defined on the object level
--let $mrs_add_db_object=func_sum
--let $mrs_add_db_object_path=/func_sum_md
--let $mrs_add_db_object_type=FUNCTION
--let $mrs_add_db_object_format=ITEM
--let $mrs_add_db_object_metadata='{"app_specific_md":"function level"}'
--source ../include/mrs/db_object/add.inc

--echo Create a schema endpoint with metadata defined on the schema level
--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic_schema_md
--let $mrs_add_schema_metadata='{"app_specific_md":"schema level"}'
--source ../include/mrs/db_schema/add.inc

--echo # Create a table endpoint with NO metadata defined on the object level
--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/tab_no_md
--source ../include/mrs/db_object/add.inc

--echo # Create a table endpoint with metadata defined on the object level
--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/tab_md
--let $mrs_add_db_object_metadata='{"app_specific_md":"table level"}'
--source ../include/mrs/db_object/add.inc

--echo # Create a view endpoint with NO metadata defined on the object level
--let $mrs_add_db_object=view1
--let $mrs_add_db_object_path=/view_no_md
--source ../include/mrs/db_object/add.inc

--echo # Create a view endpoint with metadata defined on the object level
--let $mrs_add_db_object=view1
--let $mrs_add_db_object_path=/view_md
--let $mrs_add_db_object_metadata='{"app_specific_md":"view level"}'
--source ../include/mrs/db_object/add.inc

--echo # Create a procedure endpoint with NO metadata defined on the object level
--let $mrs_add_db_object=proc_int
--let $mrs_add_db_object_path=/proc_int_no_md
--let $mrs_add_db_object_type=PROCEDURE
--source ../include/mrs/db_object/add.inc

--echo # Create a procedure endpoint with metadata defined on the object level
--let $mrs_add_db_object=proc_int
--let $mrs_add_db_object_path=/proc_int_md
--let $mrs_add_db_object_type=PROCEDURE
--let $mrs_add_db_object_metadata='{"app_specific_md":"procedure level"}'
--source ../include/mrs/db_object/add.inc

--echo
--echo Create a service requiring authentication
--echo
--let $mrs_add_service_path="/svc_req_auth"
--let $mrs_add_host_name=""
--let $mrs_add_service_metadata='{"app_specific_md":"service level"}'
--source ../include/mrs/service/add.inc

--let $mrs_add_auth_app=default authentication
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/svc_req_auth
--let $mrs_add_auth_registered_users_only=1
--source ../include/mrs/auth_app/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/basic_schema
--let $mrs_add_schema_metadata='{"app_specific_md":"schema level"}'
--let $mrs_add_schema_auth=TRUE
--source ../include/mrs/db_schema/add.inc

# Add endpoints to the service requiring authentication
--echo # Create a function endpoint
# This endpoint only exercises metadata and has no backing function.
--let $mrs_data_provider_allow_missing_object=1
--let $mrs_add_db_object=func_sum
--let $mrs_add_db_object_path=/func_sum
--let $mrs_add_db_object_type=FUNCTION
--let $mrs_add_db_object_format=ITEM
--let $mrs_add_db_object_metadata='{"app_specific_md":"function level"}'
--source ../include/mrs/db_object/add.inc

--echo # Create a table endpoint with metadata defined on the object level
--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/tab
--let $mrs_add_db_object_metadata='{"app_specific_md":"table level"}'
--source ../include/mrs/db_object/add.inc

--echo # Create a view endpoint with metadata defined on the object level
--let $mrs_add_db_object=view1
--let $mrs_add_db_object_path=/view
--let $mrs_add_db_object_metadata='{"app_specific_md":"view level"}'
--source ../include/mrs/db_object/add.inc

--echo # Create a procedure endpoint with metadata defined on the object level
--let $mrs_add_db_object=proc_int
--let $mrs_add_db_object_path=/proc_int
--let $mrs_add_db_object_type=PROCEDURE
--let $mrs_add_db_object_metadata='{"app_specific_md":"procedure level"}'
--source ../include/mrs/db_object/add.inc

--source ../include/mrs/end_object_definition.inc

# add a "root" user to mrs_users and assign a proper role to be able to access
# REST objects
insert into mysql_rest_service_metadata.mrs_user(id, auth_app_id, name,
vendor_user_id, login_permitted)
values (0x11EFD1DC9D62CD2E89FABBBF8E89057E,
(select id from mysql_rest_service_metadata.auth_app limit 1),
"user_valid", "user_valid@%", 1);

insert into mysql_rest_service_metadata.mrs_user_has_role(user_id, role_id)
values (0x11EFD1DC9D62CD2E89FABBBF8E89057E, 0x31000000000000000000000000000000);

exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/proc_int_md
    --wait-until-status=60 --display REQUEST;

--echo
--echo 1. Check the _metadata on service level
--echo

--echo 1.1 Check the _metadata for a service that has it set
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 1.2 Check the _metadata for a service that has it NULL
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_no_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 1.3 Check the forbidden PUT method for service _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/_metadata
    -t PUT --payload '{}'  --expected-status Forbidden;

--echo 1.4 Check the forbidden POST method for service _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/_metadata
    -t POST --expected-status Forbidden;

--echo 1.5 Check the forbidden DELETE method for service _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/_metadata
    -t DELETE --expected-status Forbidden;

--echo 1.6 Check the OPTIONS method for service _metadata
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/_metadata
    --display REQUEST,HEADER -t OPTIONS --response-type RAW;

--echo
--echo 2. Check the _metadata on schema level
--echo

--echo 2.1 Check the _metadata for a schema that has it set
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 2.2 Check the _metadata for a schema that has it NULL
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/func_schema_no_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 2.3 Check the forbidden PUT method for schema _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/_metadata
    -t PUT --payload '{}'  --expected-status Forbidden;

--echo 2.4 Check the forbidden POST method for schema _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/_metadata
    -t POST --expected-status Forbidden;

--echo 2.5 Check the forbidden DELETE method for schema _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/_metadata
    -t DELETE --expected-status Forbidden;

--echo
--echo 3, Check the _metadata on object level
--echo

--echo 3.1 Check the _metadata for a FUNCTION object that has it set
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/func_schema_no_md/func_sum_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 3.2 Check the _metadata for a FUNCTION object that has it NULL
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/func_schema_no_md/func_sum_no_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 3.3 Check the _metadata for a TABLE object that has it set
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/tab_md/_metadata
  --display REQUEST,HEADER,BODY;

--echo 3.4 Check the _metadata for a TABLE object that has it NULL
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/tab_no_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 3.5 Check the _metadata for a VIEW object that has it set
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/view_md/_metadata
  --display REQUEST,HEADER,BODY;

--echo 3.6 Check the _metadata for a VIEW object that has it NULL
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/view_no_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 3.7 Check the _metadata for a PROCEDURE object that has it set
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/proc_int_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 3.8 Check the _metadata for a PROCEDURE object that has it NULL
--replace_regex /^Date.*/Date -> %DATE%/
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/proc_int_no_md/_metadata
    --display REQUEST,HEADER,BODY;

--echo 3.9 Check the forbidden PUT method for object _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/tab_md/_metadata
    -t PUT --payload '{}'  --expected-status Forbidden;

--echo 3.10 Check the forbidden POST method for object _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/tab_md/_metadata
    -t POST --expected-status Forbidden;

--echo 3.11 Check the forbidden DELETE method for object _metadata
exec $MRS_CLIENT_ARGS --path /svc_md/basic_schema_md/tab_md/_metadata
    -t DELETE --expected-status Forbidden;

--echo
--echo 4. Check the _metadata for the service that requires authentication for authorized user
--echo

# get the session file
exec $MRS_CLIENT_ARGS
  -a BASIC
  --path /svc_req_auth/authentication/login
  -u user_valid
  -p pwd
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo 4.1 Authorized user can access service-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo 4.2 Authorized user can access schema-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo 4.3 Authorized user can access function object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/func_sum/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo 4.4 Authorized user can access table object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/tab/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo 4.5 Authorized user can access view object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/view/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session.dat;

--echo 4.6 Authorized user can access procedure object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/proc_int/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session.dat;


--echo
--echo 5. Check the _metadata for the service that requires authentication for not authorized user
--echo

# Revoke login rights to that user and wait for MRS to notice change
update mysql_rest_service_metadata.mrs_user set login_permitted = 0 where id = 0x11EFD1DC9D62CD2E89FABBBF8E89057E;
--source ../include/mrs/wait_mrs_read_metadata.inc

# get the session file
exec $MRS_CLIENT_ARGS
  -a BASIC
  --path /svc_req_auth/authentication/login
  -u user_valid
  -p pwd
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

--echo 5.1 Non-authorized user can still access service-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/_metadata
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

--echo 5.2 Authorized user can't access schema-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/_metadata
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

--echo 5.3 Authorized user can't access function object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/func_sum/_metadata
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

--echo 5.4 Authorized user can't access table object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/tab/_metadata
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

--echo 5.5 Authorized user can't access view object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/view/_metadata
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

--echo 5.6 Authorized user can't access procedure object-level metadata
exec $MRS_CLIENT_ARGS
  --path /svc_req_auth/basic_schema/proc_int/_metadata
  --expected-status Unauthorized
  --session-file $MYSQL_TMP_DIR/user1_session2.dat;

# Cleanup
DROP USER user_valid@'%';
remove_file $MYSQL_TMP_DIR/user1_session.dat;
remove_file $MYSQL_TMP_DIR/user1_session2.dat;
--source ../include/mrs/cleanup.inc
