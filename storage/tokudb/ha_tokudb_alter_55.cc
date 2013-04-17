#if TOKU_INCLUDE_ALTER_55

#include "ha_tokudb_alter_common.cc"

class ha_tokudb_add_index : public handler_add_index {
public:
    DB_TXN *txn;
    bool incremented_numDBs;
    bool modified_DBs;
        ha_tokudb_add_index(TABLE* table, KEY* key_info, uint num_of_keys, DB_TXN *txn, bool incremented_numDBs, bool modified_DBs) :
                handler_add_index(table, key_info, num_of_keys), txn(txn), incremented_numDBs(incremented_numDBs), modified_DBs(modified_DBs) {
    }
        ~ha_tokudb_add_index() {
    }
};

volatile int ha_tokudb_add_index_wait = 0;

int ha_tokudb::add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys, handler_add_index **add) {
    TOKUDB_DBUG_ENTER("ha_tokudb::add_index");
    while (ha_tokudb_add_index_wait) sleep(1); // debug

    int error;
    bool incremented_numDBs = false;
    bool modified_DBs = false;

    // transaction is created in prepare_for_alter
    DB_TXN* txn = transaction;

    error = tokudb_add_index(
        table_arg,
        key_info,
        num_of_keys,
        txn,
        &incremented_numDBs,
        &modified_DBs
        );
    if (error) { goto cleanup; }
    
cleanup:
    DBUG_EXECUTE_IF("add_index_fail", {
        error = 1;
    });
    if (error) {
        if (txn) {
            restore_add_index(table_arg, num_of_keys, incremented_numDBs, modified_DBs);
        }
    } else {
        *add = new ha_tokudb_add_index(table_arg, key_info, num_of_keys, txn, incremented_numDBs, modified_DBs);
    }
    TOKUDB_DBUG_RETURN(error);
}

volatile int ha_tokudb_final_add_index_wait = 0;

int ha_tokudb::final_add_index(handler_add_index *add_arg, bool commit) {
    TOKUDB_DBUG_ENTER("ha_tokudb::final_add_index");
    while (ha_tokudb_final_add_index_wait) sleep(1); // debug

    // extract the saved state variables
    ha_tokudb_add_index *add = static_cast<class ha_tokudb_add_index*>(add_arg);
    bool incremented_numDBs = add->incremented_numDBs;
    bool modified_DBs = add->modified_DBs;
    TABLE *table = add->table;
    uint num_of_keys = add->num_of_keys;
    delete add;

    int error = 0;

    DBUG_EXECUTE_IF("final_add_index_fail", {
        error = 1;
    });
    // at this point, the metadata lock ensures that the
    // newly created indexes cannot be modified,
    // regardless of whether the add index was hot.
    // Because a subsequent drop index may cause an
    // error requireing us to abort the transaction,
    // we prematurely close the added indexes, regardless
    // of whether we are committing or aborting.
    restore_add_index(table, num_of_keys, incremented_numDBs, modified_DBs);
    // transaction does not need to be committed,
    // we depend on MySQL to rollback the transaction
    // by calling tokudb_rollback

    TOKUDB_DBUG_RETURN(error);
}

volatile int ha_tokudb_prepare_drop_index_wait = 0; //debug

//
// Prepares to drop indexes to the table. For each value, i, in the array key_num,
// table->key_info[i] is a key that is to be dropped.
//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments
// Parameters:
//      [in]    table_arg - table that is being modified, seems to be identical to this->table
//      [in]    key_num - array of indexes that specify which keys of the array table->key_info
//                  are to be dropped
//              num_of_keys - size of array, key_num
//  Returns:
//      0 on success, error otherwise
//
int ha_tokudb::prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::prepare_drop_index");
    while (ha_tokudb_prepare_drop_index_wait) sleep(1); // debug

    DB_TXN *txn = transaction;
    assert(txn);
    int error = drop_indexes(table_arg, key_num, num_of_keys, table_arg->key_info, txn);
    DBUG_EXECUTE_IF("prepare_drop_index_fail", {
        error = 1;
    });

    TOKUDB_DBUG_RETURN(error);
}

volatile int ha_tokudb_final_drop_index_wait = 0; // debug

//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments, therefore, we can make
// this function just return
int ha_tokudb::final_drop_index(TABLE *table_arg) {
    TOKUDB_DBUG_ENTER("ha_tokudb::final_drop_index");
    while (ha_tokudb_final_drop_index_wait) sleep(1); // debug

    int error = 0;
    DBUG_EXECUTE_IF("final_drop_index_fail", {
        error = 1;
    });
    TOKUDB_DBUG_RETURN(error);
}

bool ha_tokudb::is_alter_table_hot() {
    TOKUDB_DBUG_ENTER("is_alter_table_hot");
    bool is_hot = false;
    THD *thd = ha_thd();
    if (get_create_index_online(thd) && thd_sql_command(thd)== SQLCOM_CREATE_INDEX) {
        // this code must match the logic in ::store_lock for hot indexing
        rw_rdlock(&share->num_DBs_lock);
        if (share->num_DBs == (table->s->keys + test(hidden_primary_key))) {
            is_hot = true;
        }
        rw_unlock(&share->num_DBs_lock);
    } 
    TOKUDB_DBUG_RETURN(is_hot);
}

int ha_tokudb::new_alter_table_frm_data(const uchar *frm_data, size_t frm_len) {
    return write_frm_data(frm_data, frm_len);
}

void ha_tokudb::prepare_for_alter() {
    TOKUDB_DBUG_ENTER("prepare_for_alter");

    // this is here because mysql commits the transaction before prepare_for_alter is called.
    // we need a transaction to add indexes, drop indexes, and write the new frm data, so we
    // create one.  this transaction will be retired by mysql alter table when it commits
    //
    // if we remove the commit before prepare_for_alter, then this is not needed.
    transaction = NULL;
    THD *thd = ha_thd();
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    assert(trx);
    // for partitioned tables, a transaction may already exist, 
    // as we call prepare_for_alter on all partitions
    if (!trx->sub_sp_level) {
        int error = create_txn(thd, trx);
        assert(error == 0);
        assert(thd->in_sub_stmt == 0);
    }
    transaction = trx->sub_sp_level;
    DBUG_VOID_RETURN;
}

bool ha_tokudb::try_hot_alter_table() {
    TOKUDB_DBUG_ENTER("try_hot_alter_table");
    THD *thd = ha_thd();
    bool disable_hot_alter = get_disable_hot_alter(thd);
    DBUG_RETURN(!disable_hot_alter);
}

#endif
