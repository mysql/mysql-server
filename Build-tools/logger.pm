#
# Create a log entry
#
sub logger
{
	my $message=$_[0];
	print timestamp() . " " . $message . "\n" if $opt_verbose;
	if (defined $opt_log && !$opt_dry_run)
	{
		open LOG, ">>$logfile" or die "Can't open logfile $logfile!";
		print LOG timestamp() . " " . $message . "\n";
		close LOG;
	}
}

# Create a time stamp for logging purposes
sub timestamp
{
	return &ymd() . " " . &hms();
}

#
# return the current time as a string (HH:MM:SS)
#
sub hms
{
	my @ta= localtime(time());
	my $h= $ta[2];
	$h= "0" . "$h" if ($h <= 9);
	my $m= $ta[1];
	$m= "0" . "$m" if ($m <= 9);
	my $s= $ta[0];
	$s="0" . "$s" if ($s <= 9);

	return "$h:$m:$s";
}

#
# return the current date as a string (YYYYMMDD)
#
sub ymd
{
	my @ta=localtime(time());
	my $d=$ta[3];
	$d="0" . "$d" if ($d <= 9);
	my $m=$ta[4]+1;
	$m="0" . "$m" if ($m <= 9);
	my $y=1900+$ta[5];

	return "$y$m$d";
}
