#if TOKU_INCLUDE_ALTER_55

bool ha_tokudb::try_hot_alter_table() {
    TOKUDB_DBUG_ENTER("try_hot_alter_table");
    THD *thd = ha_thd();
    bool disable_hot_alter = get_disable_hot_alter(thd);
    DBUG_RETURN(!disable_hot_alter);
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

int ha_tokudb::new_alter_table_frm_data(const uchar *frm_data, size_t frm_len) {
    return write_frm_data(frm_data, frm_len);
}

#endif
