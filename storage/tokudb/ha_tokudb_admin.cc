#if 0 // QQQ use default
//
// This function will probably need to be redone from scratch
// if we ever choose to implement it
//
int ha_tokudb::analyze(THD * thd, HA_CHECK_OPT * check_opt) {
    uint i;
    DB_BTREE_STAT *stat = 0;
    DB_TXN_STAT *txn_stat_ptr = 0;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd->ha_data[tokudb_hton->slot];
    DBUG_ASSERT(trx);

    for (i = 0; i < table_share->keys; i++) {
        if (stat) {
            free(stat);
            stat = 0;
        }
        if ((key_file[i]->stat) (key_file[i], trx->all, (void *) &stat, 0))
            goto err;
        share->rec_per_key[i] = (stat->bt_ndata / (stat->bt_nkeys ? stat->bt_nkeys : 1));
    }
    /* A hidden primary key is not in key_file[] */
    if (hidden_primary_key) {
        if (stat) {
            free(stat);
            stat = 0;
        }
        if ((file->stat) (file, trx->all, (void *) &stat, 0))
            goto err;
    }
    pthread_mutex_lock(&share->mutex);
    share->status |= STATUS_TOKUDB_ANALYZE;        // Save status on close
    share->version++;           // Update stat in table
    pthread_mutex_unlock(&share->mutex);
    update_status(share, table);        // Write status to file
    if (stat)
        free(stat);
    return ((share->status & STATUS_TOKUDB_ANALYZE) ? HA_ADMIN_FAILED : HA_ADMIN_OK);

  err:
    if (stat)
        free(stat);
    return HA_ADMIN_FAILED;
}
#endif

volatile int ha_tokudb_optimize_wait = 0; // debug

// flatten all DB's in this table, to do so, just do a full scan on every DB
int ha_tokudb::optimize(THD * thd, HA_CHECK_OPT * check_opt) {
    TOKUDB_DBUG_ENTER("ha_tokudb::optimize");
    while (ha_tokudb_optimize_wait) sleep(1); // debug

    int error;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
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
    TOKUDB_DBUG_RETURN(error);
}


struct check_context {
    THD *thd;
};

static int
ha_tokudb_check_progress(void *extra, float progress) {
    struct check_context *context = (struct check_context *) extra;
    int result = 0;
    if (context->thd->killed)
        result = ER_ABORTING_CONNECTION;
    return result;
}

static void
ha_tokudb_check_info(THD *thd, TABLE *table, const char *msg) {
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

volatile int ha_tokudb_check_verbose = 0; // debug
volatile int ha_tokudb_check_wait = 0; // debug

int
ha_tokudb::check(THD *thd, HA_CHECK_OPT *check_opt) {
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
        time_t now;
        char timebuf[32];
        snprintf(write_status_msg, sizeof write_status_msg, "%s primary=%d num=%d", share->table_name, primary_key, num_DBs);
        if (ha_tokudb_check_verbose) {
            ha_tokudb_check_info(thd, table, write_status_msg);
            now = time(0);
            fprintf(stderr, "%.24s ha_tokudb::check %s\n", ctime_r(&now, timebuf), write_status_msg);
        }
        for (uint i = 0; i < num_DBs; i++) {
            time_t now;
            DB *db = share->key_file[i];
            const char *kname = NULL;
            if (i == primary_key) {
                kname = "primary"; // hidden primary key does not set name
            }
            else {
                kname = table_share->key_info[i].name;
            }
            snprintf(write_status_msg, sizeof write_status_msg, "%s key=%s %u", share->table_name, kname, i);
            thd_proc_info(thd, write_status_msg);
            if (ha_tokudb_check_verbose) {
                ha_tokudb_check_info(thd, table, write_status_msg);
                now = time(0);
                fprintf(stderr, "%.24s ha_tokudb::check %s\n", ctime_r(&now, timebuf), write_status_msg);
            }
            struct check_context check_context = { thd };
            r = db->verify_with_progress(db, ha_tokudb_check_progress, &check_context, ha_tokudb_check_verbose, keep_going);
            snprintf(write_status_msg, sizeof write_status_msg, "%s key=%s %u result=%d", share->table_name, kname, i, r);
            thd_proc_info(thd, write_status_msg);
            if (ha_tokudb_check_verbose) {
                ha_tokudb_check_info(thd, table, write_status_msg);
                now = time(0);
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
