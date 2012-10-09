--echo Check the size of the vardir
--echo The output size is in unit blocks
perl;
#!/usr/bin/perl
use warnings;
use strict;
use File::Find;
my $dir = $ENV{'MYSQLTEST_VARDIR'};
my $size = 0;
find( sub { $size += -f $_ ? -s _ : 0 }, $dir );

if ( $size < 1717987000 )
        { print "TRUE", "\n";}
else   
        { print "FALSE $size", "\n";}


## Expected size for the VAR dir being changed for the PB2 runs

##if ( $size =~ /^496[\d]{5}$/ )
##	{  
##               { print "TRUE", "\n";}
##        }
##else
##	{ print "FALSE $size", "\n";}
EOF
