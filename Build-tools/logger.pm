# Helper functions

#
# Create a log entry
#
sub logger
{
	my $message= $_[0];
	my $cmnd= $_[1];

	print $message . "\n" if !$opt_quiet && !$opt_verbose && !$cmnd;
	print timestamp() . " " . $message . "\n" if $opt_verbose;
	if (defined $opt_log && !$opt_dry_run)
	{
		open LOG, ">>$LOGFILE" or die "Can't open logfile $LOGFILE!";
		print LOG timestamp() . " " . $message . "\n";
		close LOG;
	}
}

#
# run_command(<command>,<error message>)
# Execute the given command or die with the respective error message
# Just print out the command when doing a dry run
#
sub run_command
{
	my $command= $_[0];
	my $errormsg= $_[1];
	if ($opt_dry_run)
	{
		print "$command\n";
	}
	else
	{
	        &logger($command, 1);

		$command.= ';' unless ($command =~ m/^.*;$/);

		$command =~ s/;/ >> $LOGFILE 2>&1;/g if defined $opt_log;
		$command =~ s/;/ > \/dev\/null;/g if (!$opt_verbose && !$opt_log);
		system($command) == 0 or &abort("$errormsg\n");
	}
}

#
# abort(<message>)
# Exit with giving out the given error message or by sending
# it via email to the given mail address (including a log file snippet,
# if available)
#
sub abort
{
	my $message= $_[0];
	my $messagefile;
	my $subject= "Bootstrap of $REPO failed" if $opt_mail;
	$message= "ERROR: " . $message;
	&logger($message);

	if ($opt_mail && !$opt_dry_run)
	{
		$messagefile= "/tmp/message.$$";
		open(TMP,">$messagefile");
		print TMP "$message\n\n";
		close TMP;
		if (defined $opt_log)
		{
			system("tail -n 40 $LOGFILE >> $messagefile");
		}
		system("mail -s \"$subject\" $opt_mail < $messagefile");
		unlink($messagefile);
	}

	exit 1;
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
