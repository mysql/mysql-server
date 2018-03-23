# If $file is set, write $str to it. If not, write $str to stdout.
let ENV_STR  = $str;
let ENV_FILE = $file;

--perl EOF
  my $out_str  = $ENV{'ENV_STR'} or die;
  my $out_file = $ENV{'ENV_FILE'};

  if ($out_file) {
    open(OUT_FILE, ">>", $out_file) or die "Can't open $out_file for write: $!";
    print OUT_FILE "$out_str\n";
    close OUT_FILE;
  } else {
    print "$out_str\n";
  }
EOF

