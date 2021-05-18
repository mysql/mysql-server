# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

set @@persist_only.back_log=80;
set @@persist_only.binlog_gtid_simple_recovery=1;
set @@persist_only.disabled_storage_engines='';
set @@persist_only.ft_max_word_len=84;
set @@persist_only.ft_min_word_len=4;
set @@persist_only.ft_query_expansion_limit=20;
set @@persist_only.innodb_adaptive_hash_index_parts=8;
set @@persist_only.innodb_api_disable_rowlock=0;
set @@persist_only.innodb_api_enable_binlog=0;
set @@persist_only.innodb_api_enable_mdl=0;
set @@persist_only.innodb_autoinc_lock_mode=1;
set @@persist_only.innodb_buffer_pool_chunk_size=134217728;
set @@persist_only.innodb_buffer_pool_instances=1;
set @@persist_only.innodb_doublewrite=1;
set @@persist_only.innodb_force_recovery=0;
set @@persist_only.innodb_ft_cache_size=16777216;
set @@persist_only.innodb_ft_max_token_size=84;
set @@persist_only.innodb_ft_min_token_size=4;
set @@persist_only.innodb_ft_sort_pll_degree=2;
set @@persist_only.innodb_ft_total_cache_size=640000000;
set @@persist_only.innodb_log_buffer_size=16777216;
set @@persist_only.innodb_log_file_size=50331648;
set @@persist_only.innodb_log_files_in_group=2;
set @@persist_only.innodb_open_files=1;
set @@persist_only.innodb_page_cleaners=1;
set @@persist_only.innodb_purge_threads=4;
set @@persist_only.innodb_read_io_threads=4;
set @@persist_only.innodb_rollback_on_timeout=0;
set @@persist_only.innodb_sort_buffer_size=1048576;
set @@persist_only.innodb_sync_array_size=1;
set @@persist_only.innodb_use_native_aio=1;
set @@persist_only.innodb_write_io_threads=4;
set @@persist_only.log_slave_updates=0;
set @@persist_only.max_digest_length=1024;
set @@persist_only.myisam_recover_options='OFF';
set @@persist_only.ngram_token_size=2;
set @@persist_only.old=0;
set @@persist_only.open_files_limit=5000;
set @@persist_only.performance_schema=1;
set @@persist_only.performance_schema_accounts_size=-1;
set @@persist_only.performance_schema_digests_size=10000;
set @@persist_only.performance_schema_error_size=1986;
set @@persist_only.performance_schema_events_stages_history_long_size=10000;
set @@persist_only.performance_schema_events_stages_history_size=10;
set @@persist_only.performance_schema_events_statements_history_long_size=10000;
set @@persist_only.performance_schema_events_statements_history_size=10;
set @@persist_only.performance_schema_events_transactions_history_long_size=10000;
set @@persist_only.performance_schema_events_transactions_history_size=10;
set @@persist_only.performance_schema_events_waits_history_long_size=10000;
set @@persist_only.performance_schema_events_waits_history_size=10;
set @@persist_only.performance_schema_hosts_size=-1;
set @@persist_only.performance_schema_max_cond_classes=80;
set @@persist_only.performance_schema_max_cond_instances=-1;
set @@persist_only.performance_schema_max_digest_length=1024;
set @@persist_only.performance_schema_max_file_classes=80;
set @@persist_only.performance_schema_max_file_handles=32768;
set @@persist_only.performance_schema_max_file_instances=-1;
set @@persist_only.performance_schema_max_index_stat=-1;
set @@persist_only.performance_schema_max_memory_classes=460;
set @@persist_only.performance_schema_max_metadata_locks=-1;
set @@persist_only.performance_schema_max_mutex_classes=220;
set @@persist_only.performance_schema_max_mutex_instances=-1;
set @@persist_only.performance_schema_max_prepared_statements_instances=-1;
set @@persist_only.performance_schema_max_program_instances=-1;
set @@persist_only.performance_schema_max_rwlock_classes=50;
set @@persist_only.performance_schema_max_rwlock_instances=-1;
set @@persist_only.performance_schema_max_socket_classes=10;
set @@persist_only.performance_schema_max_socket_instances=-1;
set @@persist_only.performance_schema_max_sql_text_length=1024;
set @@persist_only.performance_schema_max_stage_classes=250;
set @@persist_only.performance_schema_max_statement_classes=202;
set @@persist_only.performance_schema_max_statement_stack=10;
set @@persist_only.performance_schema_max_table_handles=-1;
set @@persist_only.performance_schema_max_table_instances=-1;
set @@persist_only.performance_schema_max_table_lock_stat=-1;
set @@persist_only.performance_schema_max_thread_classes=50;
set @@persist_only.performance_schema_max_thread_instances=-1;
set @@persist_only.performance_schema_session_connect_attrs_size=512;
set @@persist_only.performance_schema_setup_actors_size=-1;
set @@persist_only.performance_schema_setup_objects_size=-1;
set @@persist_only.performance_schema_users_size=-1;
set @@persist_only.relay_log_recovery=0;
set @@persist_only.relay_log_space_limit=0;
set @@persist_only.skip_name_resolve=0;
set @@persist_only.skip_show_database=0;
set @@persist_only.slave_type_conversions='';
set @@persist_only.table_open_cache_instances=16;
set @@persist_only.thread_handling='one-thread-per-connection';
set @@persist_only.thread_stack=286720;
set @@persist_only.tls_version='TLSv1,TLSv1.1';
set @@persist_only.report_host=NULL;
set @@persist_only.report_port=21000;
set @@persist_only.report_password=NULL;
set @@persist_only.report_user=NULL;
