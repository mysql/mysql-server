# ==== Purpose ====
#
# Switches to the configured privilege protected database, if any is defined.
#
# ==== Usage ====
#
# --source include/rpl_use_privilege_db.inc
#

--let $include_filename = rpl_use_privilege_db.inc [USE $RPL_PRIV_DB]
--source include/begin_include_file.inc

if ($RPL_PRIV_DB)
{
    --disable_query_log
    --eval USE $RPL_PRIV_DB
    --enable_query_log
}

--let $include_filename = rpl_use_privilege_db.inc
--source include/end_include_file.inc
