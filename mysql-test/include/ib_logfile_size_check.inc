perl;
 my $dir = $ENV{'MYSQLD_DATADIR'};
 my $size;
 opendir(DIR, $dir) or die $!;
 while (my $file = readdir(DIR))
 {

   next unless ($file =~ m/\ib_logfile.$/);
   $size = -s "$dir/$file";
   print "The size of the ib_logfile(0/1): $size\n";
 }
 close(DIR);
 exit(0)
EOF
