# ==== Purpose ====
#
# Describe the files that match a glob pattern, for the
# rpl_csa_imr_spill_encryption_rotation* tests: their number and, for the
# first one in name order, the key id of its encryption header, the MD5 of
# the whole file and of its bytes from offset 512 (after the header), and
# its device and inode numbers. Nothing run-dependent is printed.
#
# ==== Usage ====
#
# --let $fi_glob= <perl glob pattern>
# --source suite/rpl_gtid/t/rpl_csa_imr_spill_encryption_file_info.inc
#
# Output variables:
#
#   $fi_count     Number of regular files that match.
#   $fi_key       Key id of the first file's v1 encryption header, "None" if
#                 it is not encrypted, "Error: <reason>" if it cannot be read.
#   $fi_md5       MD5 of the whole first file.
#   $fi_tail_md5  MD5 of the first file's bytes from offset 512.
#   $fi_devino    "<dev>:<ino>" of the first file.
#   All but $fi_count are empty when nothing matches.

if ($fi_glob == '')
{
  --die !!!ERROR IN TEST: set $fi_glob before sourcing rpl_csa_imr_spill_encryption_file_info.inc
}

--let _FI_GLOB= $fi_glob
--let _FI_OUT= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_fi-', UUID(), '.inc')`
perl;
  use Digest::MD5 qw(md5_hex);
  my $pattern= $ENV{'_FI_GLOB'};
  my $out= $ENV{'_FI_OUT'};
  my @files= sort grep { -f $_ } glob($pattern);
  open OUT, "> $out" or die "Error opening $out: $!";
  print OUT "--let \$fi_count= " . scalar(@files) . "\n";
  my ($key, $md5, $tail_md5, $devino)= ('', '', '', '');
  if (@files) {
    my $f= $files[0];
    open F, '<:raw', $f or die "Error opening $f: $!";
    local $/;
    my $data= <F>;
    close F;
    $data= '' unless defined $data;
    $key= 'None';
    if (length($data) >= 4 && substr($data, 0, 4) eq "\xfd\x62\x69\x6e") {
      # Version byte 1, then the first field: type 1 (key id), one length
      # byte, the key id.
      if (length($data) < 7 || substr($data, 4, 1) ne "\x01" ||
          substr($data, 5, 1) ne "\x01") {
        $key= 'Error: unexpected encryption header';
      } else {
        my $len= ord(substr($data, 6, 1));
        $key= length($data) >= 7 + $len ? substr($data, 7, $len)
                                        : 'Error: short key id';
      }
    }
    $md5= md5_hex($data);
    $tail_md5= md5_hex(length($data) > 512 ? substr($data, 512) : '');
    my @st= stat($f);
    $devino= "$st[0]:$st[1]";
  }
  print OUT "--let \$fi_key= $key\n";
  print OUT "--let \$fi_md5= $md5\n";
  print OUT "--let \$fi_tail_md5= $tail_md5\n";
  print OUT "--let \$fi_devino= $devino\n";
  close OUT or die "Error closing $out: $!";
EOF
--source $_FI_OUT
--remove_file $_FI_OUT
--let _FI_GLOB=
--let _FI_OUT=
