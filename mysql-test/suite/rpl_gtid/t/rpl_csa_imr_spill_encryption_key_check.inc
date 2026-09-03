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
  --let _KC_KEY= $kc_key
  --let _KC_FILE= $REPLICA_KEYRING_FILE_PATH
  --let _KC_OUT= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_kc-', UUID(), '.inc')`
  perl;
    my $key= $ENV{'_KC_KEY'};
    my $file= $ENV{'_KC_FILE'};
    my $out= $ENV{'_KC_OUT'};
    open F, '<:raw', $file or die "Error opening $file: $!";
    local $/;
    my $data= <F>;
    close F;
    my $listed= index($data, "\"$key\"") >= 0 ? 1 : 0;
    open OUT, "> $out" or die "Error opening $out: $!";
    print OUT "--let \$kc_listed= $listed\n";
    close OUT or die "Error closing $out: $!";
EOF
  --source $_KC_OUT
  --remove_file $_KC_OUT
  --let _KC_KEY=
  --let _KC_FILE=
  --let _KC_OUT=
}
