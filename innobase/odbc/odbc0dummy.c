/* Used to screen off linker 'undefined symbol' errors
when making an ODBC client library */


unsigned long srv_test_cache_evict;

void buf_frame_alloc(void)
{}

void buf_frame_free(void)
{}

void trx_create(void)
{}		

void* kernel_mutex_temp;

void trx_sig_send(void)
{}

void rec_sprintf(void)
{}

void dtuple_sprintf(void)
{}

void pars_write_query_param_info (void)
{}

void que_graph_try_free (void)
{}

void pars_sql (void)
{}

void que_run_threads (void)
{}

void que_fork_start_command(void)
{}

void dict_procedure_add_to_cache(void)
{}

void dict_mem_procedure_create(void)
{}

void pars_proc_read_input_params_from_buf(void)
{}

void dict_procedure_reserve_parsed_copy(void)
{}

void* srv_sys;

void* srv_print_latch_waits;

void* srv_spin_wait_delay;

void* srv_n_spin_wait_rounds;

void* buf_debug_prints;
