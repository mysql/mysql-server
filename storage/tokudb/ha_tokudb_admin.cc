volatile int ha_tokudb_analyze_wait = 0; // debug

int ha_tokudb::analyze(THD *thd, HA_CHECK_OPT *check_opt) {
    TOKUDB_DBUG_ENTER("ha_tokudb::analyze");
    while (ha_tokudb_analyze_wait) sleep(1); // debug concurrency issues
    uint64_t rec_per_key[table_share->key_parts];
    int result = HA_ADMIN_OK;
    DB_TXN *txn = transaction;
    if (!txn)
        result = HA_ADMIN_FAILED;
    if (result == HA_ADMIN_OK) {
        uint next_key_part = 0;
        // compute cardinality for each key
        for (uint i = 0; result == HA_ADMIN_OK && i < table_share->keys; i++) {
            KEY *key_info = &table_share->key_info[i];
            uint64_t num_key_parts = get_key_parts(key_info);
            int error = analyze_key(thd, txn, i, key_info, num_key_parts, &rec_per_key[next_key_part]);
            if (error) {
                result = HA_ADMIN_FAILED;
            } else {
                // debug
                if (tokudb_debug & TOKUDB_DEBUG_ANALYZE) {
                    fprintf(stderr, "ha_tokudb::analyze %s.%s.%s ", 
                            table_share->db.str, table_share->table_name.str, i == primary_key ? "primary" : table_share->key_info[i].name);
                    for (uint j = 0; j < num_key_parts; j++) 
                        fprintf(stderr, "%lu ", rec_per_key[next_key_part+j]);
                    fprintf(stderr, "\n");
                }
            }
            next_key_part += num_key_parts;
        } 
    }
    if (result == HA_ADMIN_OK)
        share->set_card_in_status(txn, table_share->key_parts, rec_per_key);    
    TOKUDB_DBUG_RETURN(result);
}

// Compute records per key for all key parts of the ith key of the table.
// For each key part, put records per key part in *rec_per_key_part[key_part_index].
// Returns 0 if success, otherwise an error number.
// TODO statistical dives into the FT
int ha_tokudb::analyze_key(THD *thd, DB_TXN *txn, uint key_i, KEY *key_info, uint64_t num_key_parts, uint64_t *rec_per_key_part) {
    TOKUDB_DBUG_ENTER("ha_tokudb::analyze_key");
    int error = 0;
    DB *db = share->key_file[key_i];
    DBC *cursor = NULL;
    error = db->cursor(db, txn, &cursor, 0);
    if (error == 0) {
        uint64_t rows = 0;
        uint64_t unique_rows[num_key_parts];
        for (uint64_t i = 0; i < num_key_parts; i++)
            unique_rows[i] = 1;
        // stop looking when the entire dictionary was analyzed, or a cap on execution time was reached, or the analyze was killed.
        DBT key = {}; key.flags = DB_DBT_REALLOC;
        DBT prev_key = {}; prev_key.flags = DB_DBT_REALLOC;
        time_t t_start = time(0);
        while (1) {
            error = cursor->c_get(cursor, &key, 0, DB_NEXT);
            if (error != 0) {
                if (error == DB_NOTFOUND)
                    error = 0; // eof is not an error
                break;
            }
            rows++;
            // first row is a unique row, otherwise compare with the previous key
            bool copy_key = false;
            if (rows == 1) {
                copy_key = true;
            } else {
                // compare this key with the previous key.  ignore appended PK for SK's.
                // TODO if a prefix is different, then all larger keys that include the prefix are also different.
                // TODO if we are comparing the entire primary key or the entire unique secondary key, then the cardinality must be 1,
                // so we can avoid computing it.
                for (uint64_t i = 0; i < num_key_parts; i++) {
                    int cmp = tokudb_cmp_dbt_key_parts(db, &prev_key, &key, i+1);
                    if (cmp != 0) {
                        unique_rows[i]++;
                        copy_key = true;
                    }
                }
            }
            // prev_key = key
            if (copy_key) {
                prev_key.data = realloc(prev_key.data, key.size);
                assert(prev_key.data);
                prev_key.size = key.size;
                memcpy(prev_key.data, key.data, prev_key.size);
            }
            // check for limit
            if ((rows % 1000) == 0) {
                if (thd->killed) {
                    error = ER_ABORTING_CONNECTION;
                    break;
                }
                time_t t_now = time(0);
                time_t t_limit = get_analyze_time(thd);
                if (t_limit > 0 && t_now - t_start > t_limit)
                    break;
                float progress_rows = 0.0;
                if (share->rows > 0)
                    progress_rows = (float) rows / (float) share->rows;
                float progress_time = 0.0;
                if (t_limit > 0)
                    progress_time = (float) (t_now - t_start) / (float) t_limit;
                sprintf(write_status_msg, "%s.%s.%s %u of %u %.lf%% rows %.lf%% time", 
                        table_share->db.str, table_share->table_name.str, key_i == primary_key ? "primary" : table_share->key_info[key_i].name,
                        key_i, table_share->keys, progress_rows * 100.0, progress_time * 100.0);
                thd_proc_info(thd, write_status_msg);
            }
        }
        // cleanup
        free(key.data);
        free(prev_key.data);
        int close_error = cursor->c_close(cursor);
        assert(close_error == 0);
        // return cardinality
        if (error == 0) {
            for (uint64_t i = 0; i < num_key_parts; i++)
                rec_per_key_part[i]  = rows / unique_rows[i];
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

static int hot_poll_fun(void *extra, float progress) {
    HOT_OPTIMIZE_CONTEXT context = (HOT_OPTIMIZE_CONTEXT)extra;
    if (context->thd->killed) {
        sprintf(context->write_status_msg, "The process has been killed, aborting hot optimize.");
        return ER_ABORTING_CONNECTION;
    }
    float percentage = progress * 100;
    sprintf(context->write_status_msg, "Optimization of index %u of %u about %.lf%% done", context->current_table + 1, context->num_tables, percentage);
    thd_proc_info(context->thd, context->write_status_msg);
#ifdef HA_TOKUDB_HAS_THD_PROGRESS
    if (context->progress_stage < context->current_table) {
        // the progress stage is behind the current table, so move up
        // to the next stage and set the progress stage to current.
        thd_progress_next_stage(context->thd);
        context->progress_stage = context->current_table;
    }
    // the percentage we report here is for the current stage/db
    thd_progress_report(context->thd, (unsigned long long) percentage, 100);
#endif
    return 0;
}

volatile int ha_tokudb_optimize_wait = 0; // debug

// flatten all DB's in this table, to do so, peform hot optimize on each db
int ha_tokudb::optimize(THD * thd, HA_CHECK_OPT * check_opt) {
    TOKUDB_DBUG_ENTER("ha_tokudb::optimize");
    while (ha_tokudb_optimize_wait) sleep(1); // debug

    int error;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);

#ifdef HA_TOKUDB_HAS_THD_PROGRESS
    // each DB is its own stage. as HOT goes through each db, we'll
    // move on to the next stage.
    thd_progress_init(thd, curr_num_DBs);
#endif

    //
    // for each DB, run optimize and hot_optimize
    //
    for (uint i = 0; i < curr_num_DBs; i++) {
        DB* db = share->key_file[i];
        error = db->optimize(db);
        if (error) {
            goto cleanup;
        }
        struct hot_optimize_context hc;
        memset(&hc, 0, sizeof hc);
        hc.thd = thd;
        hc.write_status_msg = this->write_status_msg;
        hc.ha = this;
        hc.current_table = i;
        hc.num_tables = curr_num_DBs;
        error = db->hot_optimize(db, hot_poll_fun, &hc);
        if (error) {
            goto cleanup;
        }
    }

    error = 0;
cleanup:

#ifdef HA_TOKUDB_HAS_THD_PROGRESS
    thd_progress_end(thd);
#endif

    TOKUDB_DBUG_RETURN(error);
}

struct check_context {
    THD *thd;
};

static int ha_tokudb_check_progress(void *extra, float progress) {
    struct check_context *context = (struct check_context *) extra;
    int result = 0;
    if (context->thd->killed)
        result = ER_ABORTING_CONNECTION;
    return result;
}

static void ha_tokudb_check_info(THD *thd, TABLE *table, const char *msg) {
    if (thd->vio_ok()) {
        char tablename[256];
        snprintf(tablename, sizeof tablename, "%s.%s", table->s->db.str, table->s->table_name.str);
        thd->protocol->prepare_for_resend();
        thd->protocol->store(tablename, strlen(tablename), system_charset_info);
        thd->protocol->store("check", 5, system_charset_info);
        thd->protocol->store("info", 4, system_charset_info);
        thd->protocol->store(msg, strlen(msg), system_charset_info);
        thd->protocol->write();
    }
}

volatile int ha_tokudb_check_wait = 0; // debug

int ha_tokudb::check(THD *thd, HA_CHECK_OPT *check_opt) {
    TOKUDB_DBUG_ENTER("check");
    while (ha_tokudb_check_wait) sleep(1); // debug

    const char *old_proc_info = thd->proc_info;
    thd_proc_info(thd, "tokudb::check");

    int result = HA_ADMIN_OK;
    int r;

    int keep_going = 1;
    if (check_opt->flags & T_QUICK) {
        keep_going = 0;
    }
    if (check_opt->flags & T_EXTEND) {
        keep_going = 1;
    }

    r = acquire_table_lock(transaction, lock_write);
    if (r != 0)
        result = HA_ADMIN_INTERNAL_ERROR;
    if (result == HA_ADMIN_OK) {
        uint32_t num_DBs = table_share->keys + test(hidden_primary_key);
        snprintf(write_status_msg, sizeof write_status_msg, "%s primary=%d num=%d", share->table_name, primary_key, num_DBs);
        if (tokudb_debug & TOKUDB_DEBUG_CHECK) {
            ha_tokudb_check_info(thd, table, write_status_msg);
            time_t now = time(0);
            char timebuf[32];
            fprintf(stderr, "%.24s ha_tokudb::check %s\n", ctime_r(&now, timebuf), write_status_msg);
        }
        for (uint i = 0; i < num_DBs; i++) {
            DB *db = share->key_file[i];
            const char *kname = i == primary_key ? "primary" : table_share->key_info[i].name;
            snprintf(write_status_msg, sizeof write_status_msg, "%s key=%s %u", share->table_name, kname, i);
            thd_proc_info(thd, write_status_msg);
            if (tokudb_debug & TOKUDB_DEBUG_CHECK) {
                ha_tokudb_check_info(thd, table, write_status_msg);
                time_t now = time(0);
                char timebuf[32];
                fprintf(stderr, "%.24s ha_tokudb::check %s\n", ctime_r(&now, timebuf), write_status_msg);
            }
            struct check_context check_context = { thd };
            r = db->verify_with_progress(db, ha_tokudb_check_progress, &check_context, (tokudb_debug & TOKUDB_DEBUG_CHECK) != 0, keep_going);
            snprintf(write_status_msg, sizeof write_status_msg, "%s key=%s %u result=%d", share->table_name, kname, i, r);
            thd_proc_info(thd, write_status_msg);
            if (tokudb_debug & TOKUDB_DEBUG_CHECK) {
                ha_tokudb_check_info(thd, table, write_status_msg);
                time_t now = time(0);
                char timebuf[32];
                fprintf(stderr, "%.24s ha_tokudb::check %s\n", ctime_r(&now, timebuf), write_status_msg);
            }
            if (result == HA_ADMIN_OK && r != 0) {
                result = HA_ADMIN_CORRUPT;
                if (!keep_going)
                    break;
            }
        }
    }
    thd_proc_info(thd, old_proc_info);
    TOKUDB_DBUG_RETURN(result);
}
