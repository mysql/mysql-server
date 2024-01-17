# To use:
# set persist_only_admin_x509_subject_vars_result to the error expected
# e.g.
#
# let $persist_only_admin_x509_subject_vars_result= 0;
# source ../inc/persist_only_admin_x509_subject_vars.inc;
#
# Note: only tests the plugin vars for plugins loaded by default.

--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY auto_generate_certs = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY basedir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY bind_address = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY character_sets_dir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY datadir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY ft_stopword_file = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY init_file = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY keyring_operations = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY lc_messages_dir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY log_error = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY pid_file = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY plugin_dir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY port = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY relay_log = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY secure_file_priv = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY skip_networking = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY replica_load_tmpdir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY socket = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY tmpdir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY caching_sha2_password_auto_generate_rsa_keys = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY caching_sha2_password_private_key_path = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY caching_sha2_password_public_key_path = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_buffer_pool_load_at_startup = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_data_file_path = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_data_home_dir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_dedicated_server = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_directories = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_log_group_home_dir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_page_size = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_read_only = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_temp_data_file_path = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_temp_tablespaces_dir = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_undo_directory = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_undo_tablespaces = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY sha256_password_auto_generate_rsa_keys = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY sha256_password_private_key_path = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY sha256_password_public_key_path = DEFAULT;
--error $persist_only_admin_x509_subject_vars_result
SET PERSIST_ONLY innodb_force_load_corrupted = DEFAULT;

--echo # Following variables will never be settable

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY innodb_version = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY persisted_globals_load = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY character_set_system = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY core_file = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY have_statement_timeout = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY have_symlink = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY hostname = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY large_files_support = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY large_page_size = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY license = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY log_bin = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY log_bin_basename = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY log_bin_index = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY lower_case_file_system = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY protocol_version = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY relay_log_basename = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY relay_log_index = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY server_uuid = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY skip_external_locking = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY system_time_zone = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY version_comment = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY version_compile_machine = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY version_compile_os = DEFAULT;
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY version_compile_zlib = DEFAULT;
