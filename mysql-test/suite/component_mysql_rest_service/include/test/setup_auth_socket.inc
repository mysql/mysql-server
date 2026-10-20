--source include/have_mysql_rest_service_component.inc
--source include/not_windows.inc
--source include/not_mac_os.inc
--source include/not_solaris.inc
--source include/have_plugin_auth.inc
--source include/suppress_tls_off.inc

# BUG#40031833: HTTP credentials must not authenticate using mysqld's OS identity.
--source ../cmrs/initialize.inc

if (`SELECT @@bind_address = '::1'`)
{
  --let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://[::1]:$HTTP_SERVER_PORT
}
--let $mrs_auth_socket_client_args=$MRS_CLIENT_ARGS

# The pid file belongs to mysqld's OS user, which need not be $USER.
--let MRS_SOCKET_PID_FILE= `SELECT @@pid_file`
--perl
my @pid_stat = stat($ENV{MRS_SOCKET_PID_FILE}) or die "Cannot stat mysqld pid file: $!";
my $os_user = getpwuid($pid_stat[4]);
defined $os_user or die "Cannot resolve mysqld OS user";
my $other_user = "${os_user}_other";
$os_user ne 'mrs_socket_other' or die "OS user must differ from mrs_socket_other";
for ($os_user, $other_user) {
  s/\\/\\\\/g;
  s/'/''/g;
}
open(my $out, '>', "$ENV{MYSQL_TMP_DIR}/mrs_auth_socket_users.inc") or die $!;
print $out "--let \$mrs_socket_os_user=$os_user\n";
print $out "--let \$mrs_socket_other_os_user=$other_user\n";
close($out) or die $!;
EOF
--source $MYSQL_TMP_DIR/mrs_auth_socket_users.inc
--remove_file $MYSQL_TMP_DIR/mrs_auth_socket_users.inc

--replace_result $mrs_socket_os_user MYSQLD_OS_USER
eval CREATE USER mrs_socket_match@localhost
  IDENTIFIED WITH auth_socket AS '$mrs_socket_os_user';
--replace_result $mrs_socket_other_os_user OTHER_OS_USER
eval CREATE USER mrs_socket_other@localhost
  IDENTIFIED WITH auth_socket AS '$mrs_socket_other_os_user';
CREATE USER mrs_socket_password@'%'
  IDENTIFIED WITH caching_sha2_password BY 'correct-password';

# This existing test plugin verifies the password, sets external_user, and
# authenticates as the account named by its authentication string.
CREATE USER mrs_socket_external@'%' IDENTIFIED WITH test_plugin_server
  AS 'mrs_socket_proxy';
CREATE USER mrs_socket_proxy@'%' IDENTIFIED BY 'proxy-password';
GRANT PROXY ON mrs_socket_proxy@'%' TO mrs_socket_external@'%';

CREATE SCHEMA mrs_auth_socket;
CREATE TABLE mrs_auth_socket.fixture (id INT PRIMARY KEY, marker VARCHAR(64));
INSERT INTO mrs_auth_socket.fixture VALUES (73, 'auth-socket-fixture');
--let $router_test_schemas=mrs_auth_socket;$router_test_schemas

--source ../../../router/include/mrs/start_object_definition.inc

--let $mrs_add_service_path="/svc"
--let $mrs_add_host_name=""
--source ../../../router/include/mrs/service/add.inc

--let $mrs_add_schema=mrs_auth_socket
--let $mrs_add_schema_path=/db
--let $mrs_add_schema_auth=TRUE
--source ../../../router/include/mrs/db_schema/add.inc

--let $mrs_add_db_object=fixture
--let $mrs_add_db_object_path=/fixture
--let $mrs_add_db_object_op=READ
--let $mrs_add_db_object_auth=TRUE
--source ../../../router/include/mrs/db_object/add.inc

# Permit all authenticated MySQL accounts, including both auth_socket accounts.
--let $mrs_add_auth_app=mysql_auth
--let $mrs_add_auth_vendor=MySQL Internal
--let $mrs_add_auth_service=/svc
--let $mrs_add_auth_registered_users_only=0
--let $mrs_add_auth_role_name=Full Access
--source ../../../router/include/mrs/auth_app/add.inc

--source ../../../router/include/mrs/end_object_definition.inc
