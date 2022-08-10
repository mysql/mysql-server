#
# Tests for WL#12086: Cryptographically signed SET statements
#
# Testing FR1 and FR1.5.
# Must be empty
SELECT @@GLOBAL.persist_only_admin_x509_subject;
@@GLOBAL.persist_only_admin_x509_subject

SELECT @@SESSION.persist_only_admin_x509_subject;
ERROR HY000: Variable 'persist_only_admin_x509_subject' is a GLOBAL variable
# Testing FR1.1.
SET GLOBAL persist_only_admin_x509_subject = 'haha';
ERROR HY000: Variable 'persist_only_admin_x509_subject' is a read only variable
SET SESSION persist_only_admin_x509_subject = 'haha';
ERROR HY000: Variable 'persist_only_admin_x509_subject' is a read only variable
# Testing FR1.2.
SELECT CHARSET(@@global.persist_only_admin_x509_subject);
CHARSET(@@global.persist_only_admin_x509_subject)
utf8mb3
# Testing FR1.4.
SET PERSIST_ONLY persist_only_admin_x509_subject = 'persist_only';
ERROR HY000: Variable 'persist_only_admin_x509_subject' is a non persistent read only variable
# Testing FR3
# Must be empty
SELECT @@GLOBAL.persist_only_admin_x509_subject;
@@GLOBAL.persist_only_admin_x509_subject

SET PERSIST_ONLY auto_generate_certs = DEFAULT;
ERROR HY000: Variable 'auto_generate_certs' is a non persistent read only variable
SET PERSIST_ONLY basedir = DEFAULT;
ERROR HY000: Variable 'basedir' is a non persistent read only variable
SET PERSIST_ONLY bind_address = DEFAULT;
ERROR HY000: Variable 'bind_address' is a non persistent read only variable
SET PERSIST_ONLY character_sets_dir = DEFAULT;
ERROR HY000: Variable 'character_sets_dir' is a non persistent read only variable
SET PERSIST_ONLY datadir = DEFAULT;
ERROR HY000: Variable 'datadir' is a non persistent read only variable
SET PERSIST_ONLY default_authentication_plugin = DEFAULT;
ERROR HY000: Variable 'default_authentication_plugin' is a non persistent read only variable
SET PERSIST_ONLY ft_stopword_file = DEFAULT;
ERROR HY000: Variable 'ft_stopword_file' is a non persistent read only variable
SET PERSIST_ONLY init_file = DEFAULT;
ERROR HY000: Variable 'init_file' is a non persistent read only variable
SET PERSIST_ONLY keyring_operations = DEFAULT;
ERROR HY000: Variable 'keyring_operations' is a non persistent variable
SET PERSIST_ONLY lc_messages_dir = DEFAULT;
ERROR HY000: Variable 'lc_messages_dir' is a non persistent read only variable
SET PERSIST_ONLY log_bin_use_v1_row_events = DEFAULT;
ERROR HY000: Variable 'log_bin_use_v1_row_events' is a non persistent variable
SET PERSIST_ONLY log_error = DEFAULT;
ERROR HY000: Variable 'log_error' is a non persistent read only variable
SET PERSIST_ONLY pid_file = DEFAULT;
ERROR HY000: Variable 'pid_file' is a non persistent read only variable
SET PERSIST_ONLY plugin_dir = DEFAULT;
ERROR HY000: Variable 'plugin_dir' is a non persistent read only variable
SET PERSIST_ONLY port = DEFAULT;
ERROR HY000: Variable 'port' is a non persistent read only variable
SET PERSIST_ONLY relay_log = DEFAULT;
ERROR HY000: Variable 'relay_log' is a non persistent read only variable
SET PERSIST_ONLY relay_log_info_file = DEFAULT;
ERROR HY000: Variable 'relay_log_info_file' is a non persistent read only variable
SET PERSIST_ONLY secure_file_priv = DEFAULT;
ERROR HY000: Variable 'secure_file_priv' is a non persistent read only variable
SET PERSIST_ONLY skip_networking = DEFAULT;
ERROR HY000: Variable 'skip_networking' is a non persistent read only variable
SET PERSIST_ONLY replica_load_tmpdir = DEFAULT;
ERROR HY000: Variable 'replica_load_tmpdir' is a non persistent read only variable
SET PERSIST_ONLY socket = DEFAULT;
ERROR HY000: Variable 'socket' is a non persistent read only variable
SET PERSIST_ONLY tmpdir = DEFAULT;
ERROR HY000: Variable 'tmpdir' is a non persistent read only variable
SET PERSIST_ONLY caching_sha2_password_auto_generate_rsa_keys = DEFAULT;
ERROR HY000: Variable 'caching_sha2_password_auto_generate_rsa_keys' is a non persistent read only variable
SET PERSIST_ONLY caching_sha2_password_private_key_path = DEFAULT;
ERROR HY000: Variable 'caching_sha2_password_private_key_path' is a non persistent read only variable
SET PERSIST_ONLY caching_sha2_password_public_key_path = DEFAULT;
ERROR HY000: Variable 'caching_sha2_password_public_key_path' is a non persistent read only variable
SET PERSIST_ONLY innodb_buffer_pool_load_at_startup = DEFAULT;
ERROR HY000: Variable 'innodb_buffer_pool_load_at_startup' is a non persistent read only variable
SET PERSIST_ONLY innodb_data_file_path = DEFAULT;
ERROR HY000: Variable 'innodb_data_file_path' is a non persistent read only variable
SET PERSIST_ONLY innodb_data_home_dir = DEFAULT;
ERROR HY000: Variable 'innodb_data_home_dir' is a non persistent read only variable
SET PERSIST_ONLY innodb_dedicated_server = DEFAULT;
ERROR HY000: Variable 'innodb_dedicated_server' is a non persistent read only variable
SET PERSIST_ONLY innodb_directories = DEFAULT;
ERROR HY000: Variable 'innodb_directories' is a non persistent read only variable
SET PERSIST_ONLY innodb_log_group_home_dir = DEFAULT;
ERROR HY000: Variable 'innodb_log_group_home_dir' is a non persistent read only variable
SET PERSIST_ONLY innodb_page_size = DEFAULT;
ERROR HY000: Variable 'innodb_page_size' is a non persistent read only variable
SET PERSIST_ONLY innodb_read_only = DEFAULT;
ERROR HY000: Variable 'innodb_read_only' is a non persistent read only variable
SET PERSIST_ONLY innodb_temp_data_file_path = DEFAULT;
ERROR HY000: Variable 'innodb_temp_data_file_path' is a non persistent read only variable
SET PERSIST_ONLY innodb_temp_tablespaces_dir = DEFAULT;
ERROR HY000: Variable 'innodb_temp_tablespaces_dir' is a non persistent read only variable
SET PERSIST_ONLY innodb_undo_directory = DEFAULT;
ERROR HY000: Variable 'innodb_undo_directory' is a non persistent read only variable
SET PERSIST_ONLY innodb_undo_tablespaces = DEFAULT;
ERROR HY000: Variable 'innodb_undo_tablespaces' is a non persistent variable
SET PERSIST_ONLY sha256_password_auto_generate_rsa_keys = DEFAULT;
ERROR HY000: Variable 'sha256_password_auto_generate_rsa_keys' is a non persistent read only variable
SET PERSIST_ONLY sha256_password_private_key_path = DEFAULT;
ERROR HY000: Variable 'sha256_password_private_key_path' is a non persistent read only variable
SET PERSIST_ONLY sha256_password_public_key_path = DEFAULT;
ERROR HY000: Variable 'sha256_password_public_key_path' is a non persistent read only variable
SET PERSIST_ONLY innodb_force_load_corrupted = DEFAULT;
ERROR HY000: Variable 'innodb_force_load_corrupted' is a non persistent read only variable
# Following variables will never be settable
SET PERSIST_ONLY innodb_version = DEFAULT;
ERROR HY000: Variable 'innodb_version' is a non persistent read only variable
SET PERSIST_ONLY persisted_globals_load = DEFAULT;
ERROR HY000: Variable 'persisted_globals_load' is a non persistent read only variable
SET PERSIST_ONLY character_set_system = DEFAULT;
ERROR HY000: Variable 'character_set_system' is a non persistent read only variable
SET PERSIST_ONLY core_file = DEFAULT;
ERROR HY000: Variable 'core_file' is a non persistent read only variable
SET PERSIST_ONLY have_statement_timeout = DEFAULT;
ERROR HY000: Variable 'have_statement_timeout' is a non persistent read only variable
SET PERSIST_ONLY have_symlink = DEFAULT;
ERROR HY000: Variable 'have_symlink' is a non persistent read only variable
SET PERSIST_ONLY hostname = DEFAULT;
ERROR HY000: Variable 'hostname' is a non persistent read only variable
SET PERSIST_ONLY large_files_support = DEFAULT;
ERROR HY000: Variable 'large_files_support' is a non persistent read only variable
SET PERSIST_ONLY large_page_size = DEFAULT;
ERROR HY000: Variable 'large_page_size' is a non persistent read only variable
SET PERSIST_ONLY license = DEFAULT;
ERROR HY000: Variable 'license' is a non persistent read only variable
SET PERSIST_ONLY log_bin = DEFAULT;
ERROR HY000: Variable 'log_bin' is a non persistent read only variable
SET PERSIST_ONLY log_bin_basename = DEFAULT;
ERROR HY000: Variable 'log_bin_basename' is a non persistent read only variable
SET PERSIST_ONLY log_bin_index = DEFAULT;
ERROR HY000: Variable 'log_bin_index' is a non persistent read only variable
SET PERSIST_ONLY lower_case_file_system = DEFAULT;
ERROR HY000: Variable 'lower_case_file_system' is a non persistent read only variable
SET PERSIST_ONLY protocol_version = DEFAULT;
ERROR HY000: Variable 'protocol_version' is a non persistent read only variable
SET PERSIST_ONLY relay_log_basename = DEFAULT;
ERROR HY000: Variable 'relay_log_basename' is a non persistent read only variable
SET PERSIST_ONLY relay_log_index = DEFAULT;
ERROR HY000: Variable 'relay_log_index' is a non persistent read only variable
SET PERSIST_ONLY server_uuid = DEFAULT;
ERROR HY000: Variable 'server_uuid' is a non persistent read only variable
SET PERSIST_ONLY skip_external_locking = DEFAULT;
ERROR HY000: Variable 'skip_external_locking' is a non persistent read only variable
SET PERSIST_ONLY system_time_zone = DEFAULT;
ERROR HY000: Variable 'system_time_zone' is a non persistent read only variable
SET PERSIST_ONLY version_comment = DEFAULT;
ERROR HY000: Variable 'version_comment' is a non persistent read only variable
SET PERSIST_ONLY version_compile_machine = DEFAULT;
ERROR HY000: Variable 'version_compile_machine' is a non persistent read only variable
SET PERSIST_ONLY version_compile_os = DEFAULT;
ERROR HY000: Variable 'version_compile_os' is a non persistent read only variable
SET PERSIST_ONLY version_compile_zlib = DEFAULT;
ERROR HY000: Variable 'version_compile_zlib' is a non persistent read only variable
