# ==== Purpose ====
#
# Inspect the in-memory relay log (IMR) spill files of the default channel:
# for every "imr_sp_*" file in the spill directory, report its size, whether
# it starts with the encryption magic or the plain binary log magic, the
# master key id named by its encryption header, and whether it contains a
# given plaintext marker. Used by the rpl_csa_imr_spill_encryption* tests.
# Nothing run-dependent is printed.
#
# A perl block globs the directory and writes --let lines into a temporary
# .inc file, which this file sources and then removes.
#
# ==== Usage ====
#
# --let $sef_spill_dir= <spill directory>
# --let $sef_marker= <plaintext marker to search for>
# --source suite/rpl_gtid/t/rpl_csa_imr_spill_encryption_files.inc
#
# Output variables:
#
#   $sef_count
#     Number of imr_sp_* files.
#
#   $sef_size_<n>, $sef_magic_<n>, $sef_key_<n>, $sef_marker_<n>, for
#   n = 1 .. $sef_count, in creation order (the hex unique id at the end of
#   the file name):
#     size   on-disk size in bytes
#     magic  "encrypted" (\xfd\x62\x69\x6e), "plain" (\xfe\x62\x69\x6e),
#            "empty" (fewer than 4 bytes) or "other"
#     key    the key id of the version 1 encryption header, "None" for a
#            file that is not encrypted, or "Error: <reason>"
#     marker 1 if the file contains $sef_marker, else 0

if ($sef_spill_dir == '')
{
  --die !!!ERROR IN TEST: set $sef_spill_dir before sourcing rpl_csa_imr_spill_encryption_files.inc
}
if ($sef_marker == '')
{
  --die !!!ERROR IN TEST: set $sef_marker before sourcing rpl_csa_imr_spill_encryption_files.inc
}

--let _SEF_SPILL_DIR= $sef_spill_dir
--let _SEF_MARKER= $sef_marker
--let _SEF_OUTPUT_FILE= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_sef-', UUID(), '.inc')`
perl;
  my $dir= $ENV{'_SEF_SPILL_DIR'};
  my $marker= $ENV{'_SEF_MARKER'};
  my $out= $ENV{'_SEF_OUTPUT_FILE'};
  my @files= grep { -f $_ } glob("$dir/imr_sp_*");
  my %id;
  foreach my $f (@files) {
    my ($hex)= $f =~ /_([0-9a-f]+)$/;
    $id{$f}= defined $hex ? hex($hex) : 0;
  }
  @files= sort { $id{$a} <=> $id{$b} } @files;
  open OUT, "> $out" or die "Error opening $out: $!";
  print OUT "--let \$sef_count= " . scalar(@files) . "\n";
  my $n= 0;
  foreach my $f (@files) {
    $n++;
    open F, '<:raw', $f or die "Error opening $f: $!";
    local $/;
    my $data= <F>;
    close F;
    $data= '' unless defined $data;
    my $magic= 'other';
    my $key= 'None';
    if (length($data) < 4) {
      $magic= 'empty';
    } elsif (substr($data, 0, 4) eq "\xfe\x62\x69\x6e") {
      $magic= 'plain';
    } elsif (substr($data, 0, 4) eq "\xfd\x62\x69\x6e") {
      $magic= 'encrypted';
      # Version byte 1, then the first field: type 1 (key id), one length
      # byte, the key id.
      if (length($data) < 7 || substr($data, 4, 1) ne "\x01") {
        $key= 'Error: unexpected encryption header version';
      } elsif (substr($data, 5, 1) ne "\x01") {
        $key= 'Error: unexpected first field type';
      } else {
        my $len= ord(substr($data, 6, 1));
        $key= length($data) >= 7 + $len ? substr($data, 7, $len)
                                        : 'Error: short key id';
      }
    }
    my $has_marker= index($data, $marker) >= 0 ? 1 : 0;
    print OUT "--let \$sef_size_$n= " . length($data) . "\n";
    print OUT "--let \$sef_magic_$n= $magic\n";
    print OUT "--let \$sef_key_$n= $key\n";
    print OUT "--let \$sef_marker_$n= $has_marker\n";
  }
  close OUT or die "Error closing $out: $!";
EOF
--source $_SEF_OUTPUT_FILE
--remove_file $_SEF_OUTPUT_FILE
--let _SEF_SPILL_DIR=
--let _SEF_MARKER=
--let _SEF_OUTPUT_FILE=
