# Verify MRS component system variables:
#
# This test validates the component's sysvars by attempting
# to set them to valid and invalid values. It ensures that:
#  - Valid values are accepted and correctly applied
#  - Invalid values are rejected with appropriate error messages
#  - Default values are correctly reported
#
# The available sysvars are:
#  - mysql_rest_service.http_port;
#  - mysql_rest_service.ssl_cert;
#  - mysql_rest_service.ssl_key;
#  - mysql_rest_service.ssl_cipher;
#  - mysql_rest_service.io_worker_threads;
#  - mysql_rest_service.http_max_connections;
#  - mysql_rest_service.http_max_request_body_size;
#  - mysql_rest_service.http_max_response_body_size;
#
# This test is split into 4 different blocks, this is the first.

--echo
--echo
--echo ## I. Validate default values of sysvars:
--echo #
--echo # 1. Ensure all sysvars report their expected default values.
--echo # 2. Validate max and min values for all integer values

# Init

--let $init_does_mrs_bootstrap=1
--source ../cmrs/initialize.inc
--source ../cmrs/wait_mrs_read_metadata.inc

--echo
--echo #
--echo # I.1 Ensure all sysvars report their expected default values.

# Check default values

# In MTR we can't use the default http_port, so reset it first.
SET GLOBAL mysql_rest_service.http_port=DEFAULT;

# http_port = 33443
--let $http_port = `SELECT @@mysql_rest_service.http_port;`
--let $http_port_default = 33443
--let $assert_text = MRS Component default http_port is 33443
--let $assert_cond = $http_port = $http_port_default
--source include/assert.inc

# ssl_cert
--let $ssl_cert_server = `SELECT @@ssl_cert;`
--let $ssl_cert = `SELECT @@mysql_rest_service.ssl_cert;`

--let $assert_text = MRS Component default ssl_cert must match MySQL Server @@ssl_cert
--let $assert_cond = "$ssl_cert" = "$ssl_cert_server"
--source include/assert.inc

# ssl_key
--let $ssl_key_server = `SELECT @@ssl_key;`
--let $ssl_key = `SELECT @@mysql_rest_service.ssl_key;`

--let $assert_text = MRS Component default ssl_key must match MySQL Server @@ssl_key
--let $assert_cond = "$ssl_key" = "$ssl_key_server"
--source include/assert.inc

# ssl_cipher
--let $ssl_cipher_matches_server = `SELECT COALESCE(@@mysql_rest_service.ssl_cipher, '') = COALESCE(@@ssl_cipher, '');`

--let $assert_text = MRS Component default ssl_cipher must match MySQL Server @@ssl_cipher
--let $assert_cond = $ssl_cipher_matches_server = 1
--source include/assert.inc

# io_worker_threads = 0
--let $io_worker_threads = `SELECT @@mysql_rest_service.io_worker_threads;`
--let $assert_text = MRS Component default io_worker_threads is 0
--let $assert_cond = $io_worker_threads = 0
--source include/assert.inc

# http_max_connections = 1024
--let $http_max_connections = `SELECT @@mysql_rest_service.http_max_connections;`
--let $assert_text = MRS Component default http_max_connections is 1024
--let $assert_cond = $http_max_connections = 1024
--source include/assert.inc

# http_max_request_body_size = 16777216
--let $http_max_request_body_size = `SELECT @@mysql_rest_service.http_max_request_body_size;`
--let $assert_text = MRS Component default http_max_request_body_size is 16777216
--let $assert_cond = $http_max_request_body_size = 16777216
--source include/assert.inc

# http_max_response_body_size = 16777216
--let $http_max_response_body_size = `SELECT @@mysql_rest_service.http_max_response_body_size;`
--let $assert_text = MRS Component default http_max_response_body_size is 16777216
--let $assert_cond = $http_max_response_body_size = 16777216
--source include/assert.inc

--echo
--echo #
--echo ## I.2. Validate max and min values for all integer values

# http_port.minimum_value=1
# http_port.maximum_value=65535

--let $http_port_min = `SELECT MIN_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_port";`

--let $assert_text = MRS Component http_port minimum value is 1
--let $assert_cond = $http_port_min = 1
--source include/assert.inc

--let $http_port_max = `SELECT MAX_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_port";`

--let $assert_text = MRS Component http_port maximum value is 65535
--let $assert_cond = $http_port_max = 65535
--source include/assert.inc

# io_worker_threads.minimum_value=0
# io_worker_threads.maximum_value=1024

--let $io_worker_threads_min = `SELECT MIN_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.io_worker_threads";`

--let $assert_text = MRS Component io_worker_threads minimum value is 0
--let $assert_cond = $io_worker_threads_min = 0
--source include/assert.inc

--let $io_worker_threads_max = `SELECT MAX_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.io_worker_threads";`

--let $assert_text = MRS Component io_worker_threads maximum value is 1024
--let $assert_cond = $io_worker_threads_max = 1024
--source include/assert.inc

# http_max_connections.minimum_value=1
# http_max_connections.maximum_value=100000

--let $http_max_connections_min = `SELECT MIN_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_max_connections";`

--let $assert_text = MRS Component http_max_connections minimum value is 1
--let $assert_cond = $http_max_connections_min = 1
--source include/assert.inc

--let $http_max_connections_max = `SELECT MAX_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_max_connections";`

--let $assert_text = MRS Component http_max_connections maximum value is 100000
--let $assert_cond = $http_max_connections_max = 100000
--source include/assert.inc

# http_max_request_body_size.minimum_value=0
# http_max_request_body_size.maximum_value=2147483648

--let $http_max_request_body_size_min = `SELECT MIN_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_max_request_body_size";`

--let $assert_text = MRS Component http_max_request_body_size minimum value is 0
--let $assert_cond = $http_max_request_body_size_min = 0
--source include/assert.inc

--let $http_max_request_body_size_max = `SELECT MAX_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_max_request_body_size";`

--let $assert_text = MRS Component http_max_request_body_size maximum value is 2147483648
--let $assert_cond = $http_max_request_body_size_max = 2147483648
--source include/assert.inc

# http_max_response_body_size.minimum_value=0
# http_max_response_body_size.maximum_value=2147483648

--let $http_max_response_body_size_min = `SELECT MIN_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_max_response_body_size";`

--let $assert_text = MRS Component http_max_response_body_size minimum value is 0
--let $assert_cond = $http_max_response_body_size_min = 0
--source include/assert.inc

--let $http_max_response_body_size_max = `SELECT MAX_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME="mysql_rest_service.http_max_response_body_size";`

--let $assert_text = MRS Component http_max_response_body_size maximum value is 2147483648
--let $assert_cond = $http_max_response_body_size_max = 2147483648
--source include/assert.inc

--echo
--echo #
--echo # Cleanup
--source ../cmrs/cleanup.inc
