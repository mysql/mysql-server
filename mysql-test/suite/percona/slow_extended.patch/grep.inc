perl;

  $file = $ENV{'grep_file'};
  $pattern = $ENV{'grep_pattern'};

  open(FILE, "$file")
    or die("Cannot open file $file: $!\n");

  $lines = 0;
  while(<FILE>) {
    $lines++ if (/$pattern/);
  }
  print "$lines\n";

  close(FILE);
EOF
