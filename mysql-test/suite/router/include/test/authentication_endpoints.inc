# Verify that authentication endpoints are created and avaiable
# despite the number of authentication application assgined to
# service.
#
## Test starts here

if ($test_param_authentication)
{
 --let $auth_path=`SELECT SUBSTRING("$test_param_authentication", 2)`
}
if (!$test_param_authentication)
{
  --let $auth_path=authentication
}

--echo
--echo
--echo ## I. Create servcices that:
--echo ## *  service A, that has nothing under it and no authApp assigned
--echo ## *  service B, that has nothing under it and one authApp assigned
--echo ## *  service C, that has nothing under it and two authApps assigned
--echo ## *  service D, that has db_object and no authApp assigned
--echo ## *  service E, that has db_object and one authApp assigned
--echo ## *  service F, that has db_object and two authApps assigned
--echo ## *  service G, that has db_object+requires_auth and no authApp assigned
--echo ## *  service H, that has db_object+requires_auth and one authApp assigned
--echo ## *  service I, that has db_object+requires_auth and two authApps assigned
--echo #
--echo # 1. Verify that under all services, endpoint /$auth_path/authApps exists and returns
--echo #    specific number of authApps in a array (WL#16694 - TSFR_2_1).
--echo # 2. Verify that under all services, endpoint /$auth_path/completed exists and returns
--echo #    valid http page.
--echo # 3. Verify that under all services, endpoint /$auth_path/login
--echo # 4. Verify that under all services, endpoint /$auth_path/logout
--echo # 5. Verify that under all services, endpoint /$auth_path/user
--echo # 6. Verify that under all services, endpoint /$auth_path/status
--echo #
--echo #
--echo ## II. Remove auth-app (WL#16694 - TSFR_2_3).
--echo # 1. Remove 'f_one-of-two' auth-app, from service "f" (verify /$auth_path/authApps)
--echo # 2. Remove all auth-apps, from service "f" (verify /$auth_path/authApps)
--echo #
--echo ## III. Remove auth-app (WL#16693 - TSFR_2_2_1).
--echo # 1. Remove 'c_one-of-two' auth-app, from service "c" (verify /authentication/authApps)
--echo # 2. Remove all auth-apps, from service "c" (verify /authentication/authApps)
--echo
--echo

--source ../predefined_setup/configure_router_mrs_root.inc
--source ../schema/basic_schema.sql

--echo #
--echo # I.1


--source ../mrs/start_object_definition.inc

############ A
--let $mrs_add_service_path="/a"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

############ B
--let $mrs_add_service_path="/b"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_auth_app=b_one
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/b
--source ../mrs/auth_app/add.inc

############ C
--let $mrs_add_service_path="/c"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_auth_app=c_one-of-two
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/c
--source ../mrs/auth_app/add.inc

--let $mrs_add_auth_app=c_two-of-two
--let $mrs_add_auth_vendor=MRS
--let $mrs_add_auth_service=/c
--source ../mrs/auth_app/add.inc

############ D
--let $mrs_add_service_path="/d"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/sch
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/t
--source ../mrs/db_object/add.inc

############ E
--let $mrs_add_service_path="/e"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/sch
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/t
--source ../mrs/db_object/add.inc

--let $mrs_add_auth_app=e_one
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/e
--source ../mrs/auth_app/add.inc

############ F
--let $mrs_add_service_path="/f"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/sch
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/t
--source ../mrs/db_object/add.inc

--let $mrs_add_auth_app=f_one-of-two
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/f
--source ../mrs/auth_app/add.inc

--let $mrs_add_auth_app=f_two-of-two
--let $mrs_add_auth_vendor=MRS
--let $mrs_add_auth_service=/f
--source ../mrs/auth_app/add.inc

############ G
--let $mrs_add_service_path="/g"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/sch
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/t
--let $mrs_add_db_object_auth=1
--source ../mrs/db_object/add.inc

############ H
--let $mrs_add_service_path="/h"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/sch
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/h
--let $mrs_add_db_object_auth=1
--source ../mrs/db_object/add.inc

--let $mrs_add_auth_app=h_one
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/h
--source ../mrs/auth_app/add.inc

############ I
--let $mrs_add_service_path="/i"
--let $mrs_add_host_name=""
if ($test_param_authentication)
{
  --let $mrs_add_service_auth_path="$test_param_authentication"
}
--source ../mrs/service/add.inc

--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/sch
--source ../mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/t
--let $mrs_add_db_object_auth=1
--source ../mrs/db_object/add.inc

--let $mrs_add_auth_app=i_one-of-two
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/i
--source ../mrs/auth_app/add.inc

--let $mrs_add_auth_app=i_two-of-two
--let $mrs_add_auth_vendor=MRS
--let $mrs_add_auth_service=/i
--source ../mrs/auth_app/add.inc

--source ../mrs/end_object_definition.inc

--echo
--echo #
--echo # I.1
--let $mrs_client_arg_path='/a/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/b/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/c/$auth_path/authApps'
--let $mrs_client_arg_display=request,status
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/two_authentication_apps_c.sch
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/d/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/e/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/f/$auth_path/authApps'
--let $mrs_client_arg_display=request,status
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/two_authentication_apps_f.sch
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/g/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/h/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/i/$auth_path/authApps'
--let $mrs_client_arg_display=request,status
--let $mrs_client_arg_json_schema_file=$SCHEMA_DIR/two_authentication_apps_i.sch
--source ../mrs/mrs_client.inc


--echo
--echo #
--echo # I.2
--let $mrs_client_arg_path='/a/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/b/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/c/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/d/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/e/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/f/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/g/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/h/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/i/$auth_path/completed'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expect_header=Content-Type
--let $mrs_client_arg_expect_header_value="text/html"
--let $mrs_client_arg_display=request,status
--source ../mrs/mrs_client.inc


--echo
--echo #
--echo # I.3
--let $mrs_client_arg_path='/a/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=TemporaryRedirect
--let $mrs_client_arg_expect_header=Location
--let $mrs_client_arg_expect_header_value="/a/$auth_path/completed?login=fail"
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/b/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=Unauthorized
--let $mrs_client_arg_expect_header=WWW-Authenticate
--let $mrs_client_arg_expect_header_value="basic"
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/c/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=TemporaryRedirect
--let $mrs_client_arg_expect_header=Location
--let $mrs_client_arg_expect_header_value="/c/$auth_path/completed?login=fail"
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/c/$auth_path/login?app=c_one-of-two'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=Unauthorized
--let $mrs_client_arg_expect_header=WWW-Authenticate
--let $mrs_client_arg_expect_header_value="basic"
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/d/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=TemporaryRedirect
--let $mrs_client_arg_expect_header=Location
--let $mrs_client_arg_expect_header_value="/d/$auth_path/completed?login=fail"
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/e/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=Unauthorized
--let $mrs_client_arg_expect_header=WWW-Authenticate
--let $mrs_client_arg_expect_header_value="basic"
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/f/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=TemporaryRedirect
--let $mrs_client_arg_expect_header=Location
--let $mrs_client_arg_expect_header_value="/f/$auth_path/completed?login=fail"
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/f/$auth_path/login?app=f_one-of-two'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=Unauthorized
--let $mrs_client_arg_expect_header=WWW-Authenticate
--let $mrs_client_arg_expect_header_value="basic"
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/g/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=TemporaryRedirect
--let $mrs_client_arg_expect_header=Location
--let $mrs_client_arg_expect_header_value="/g/$auth_path/completed?login=fail"
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/h/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=Unauthorized
--let $mrs_client_arg_expect_header=WWW-Authenticate
--let $mrs_client_arg_expect_header_value="basic"
--source ../mrs/mrs_client.inc


--let $mrs_client_arg_path='/i/$auth_path/login'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=TemporaryRedirect
--let $mrs_client_arg_expect_header=Location
--let $mrs_client_arg_expect_header_value="/i/$auth_path/completed?login=fail"
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/i/$auth_path/login?app=i_one-of-two'
--let $mrs_client_arg_response_type=raw
--let $mrs_client_arg_expected_status=Unauthorized
--let $mrs_client_arg_expect_header=WWW-Authenticate
--let $mrs_client_arg_expect_header_value="basic"
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # I.4
--let $mrs_client_arg_path='/a/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/b/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/c/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/d/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/e/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/f/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/g/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/h/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/i/$auth_path/logout'
--let $mrs_client_arg_request_type=POST
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # I.5
--let $mrs_client_arg_path='/a/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/b/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/c/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/d/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/e/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/f/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/g/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/h/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/i/$auth_path/user'
--let $mrs_client_arg_expected_status=Unauthorized
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # I.6
--let $mrs_client_arg_path='/a/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/b/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/c/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/d/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/e/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/f/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/g/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/h/$auth_path/status'
--source ../mrs/mrs_client.inc

--let $mrs_client_arg_path='/i/$auth_path/status'
--source ../mrs/mrs_client.inc


--echo
--echo #
--echo # II.1

--let $mrs_delete_auth_app="f_one-of-two"
--source ../mrs/auth_app/delete.inc
--source ../mrs/wait_mrs_read_metadata.inc

--let $mrs_client_arg_path='/f/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # II.2

--let $mrs_delete_auth_app="f_two-of-two"
--source ../mrs/auth_app/delete.inc
--source ../mrs/wait_mrs_read_metadata.inc

--let $mrs_client_arg_path='/f/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # III.1

--let $mrs_delete_auth_app="c_one-of-two"
--source ../mrs/auth_app/delete.inc
--source ../mrs/wait_mrs_read_metadata.inc

--let $mrs_client_arg_path='/c/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--echo
--echo #
--echo # III.2

--let $mrs_delete_auth_app="c_two-of-two"
--source ../mrs/auth_app/delete.inc
--source ../mrs/wait_mrs_read_metadata.inc

--let $mrs_client_arg_path='/c/$auth_path/authApps'
--source ../mrs/mrs_client.inc

--source ../mrs/cleanup.inc
--let $test_param_authentication=
