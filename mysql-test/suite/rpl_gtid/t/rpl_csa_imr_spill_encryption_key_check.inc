# ==== Purpose ====
#
# Tell whether a replication master key is in the replica's keyring. Used by
# the rpl_csa_imr_spill_encryption_rotation* tests. Run it on the replica
# connection.
#
# ==== Usage ====
#
# --let $kc_source= pfs | file
# --let $kc_key= MySQLReplicationKey_<uuid>_<n>
# --source suite/rpl_gtid/t/rpl_csa_imr_spill_encryption_key_check.inc
#
# Parameters:
#
#   $kc_source
#     pfs: look the key up in performance_schema.keyring_keys. file: search
#     $REPLICA_KEYRING_FILE_PATH for the quoted key id (the quotes keep _1
#     from matching _10 or _last_purged). The test chooses it once with a
#     probe: pfs if performance_schema.keyring_keys lists the first key.
#
#   $kc_key
#     The key id.
#
# Output variable:
#
#   $kc_listed
#     1 if the key is in the keyring, else 0.

if ($kc_key == '')
{
  --die !!!ERROR IN TEST: set $kc_key before sourcing rpl_csa_imr_spill_encryption_key_check.inc
}

if (`SELECT '$kc_source' = 'pfs'`)
{
  --let $kc_listed= `SELECT COUNT(*) FROM performance_schema.keyring_keys WHERE KEY_ID = '$kc_key'`
}
if (`SELECT '$kc_source' <> 'pfs'`)
{
  --let $grep_file= $REPLICA_KEYRING_FILE_PATH
  --let $grep_pattern= "$kc_key"
  --let $grep_output= none
  --source include/grep_pattern.inc
  --let $kc_listed= `SELECT $GREP_RESULT > 0`
  --let $grep_file=
  --let $grep_pattern=
  --let $grep_output=
}
