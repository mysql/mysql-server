# -*- cperl -*-

package My::Exec;

use strict;
use warnings;
use Carp;
use File::Basename;
use IO::File;

use base qw(Exporter);
our @EXPORT= qw(exec_print_on_error);

# Generate a logfile name from a command
sub get_logfile_name {
    my $cmd = shift;

    # Get logfile name
    my @cmd_parts = split(' ', $cmd);
    my $logfile_base = fileparse($cmd_parts[0]);
    my $logfile_name = "";
    my $log_dir = $ENV{MYSQLTEST_VARDIR};
    for my $i (1..100)
    {
        my $proposed_logfile_name = "$log_dir/$logfile_base" . '_' . $i . ".log";
        if (! -f $proposed_logfile_name)
	{
	    # We can use this file name
	    $logfile_name = $proposed_logfile_name;
            last;
	}
    }

    return $logfile_name;
}

# Save a set of lines to a file
sub save_file {
    my $filename = shift;
    my $lines    = shift;

    my $F = IO::File->new($filename, "w") or die "Can't write to '$filename': $!";
    foreach my $line (@$lines) {
        print $F $line
    }
    $F->close();
}

#
# exec_print_on_error - executes command, and prints n last lines of output
#                       from the command only if the command fails. If the command runs
#                       successfully, no output is written.
#
# Parameters:
#              cmd       - the command to run
#              max_lines - the maximum number of lines of output to show on error (default 200)
# Returns:
#              1 on success, 0 on error.
# Example:
#              use My::Exec;
#              my $res = exec_print_on_error("./mtr --suite=ndb ndb_dd_varsize");
#
sub exec_print_on_error {
    my $cmd       = shift;
    my $max_lines = shift || 200;

    my $logfile_name = get_logfile_name($cmd);

    $cmd .= " 2>&1";
    my @output = `$cmd`;
    print "Result of '$cmd': $?, logfile: '$logfile_name'\n";
    save_file($logfile_name, \@output);
    if ($? == 0)
    {
	# Test program suceeded
	return 1;
    }

    # Test program failed
    if ($? == -1)
    {
	# Failed to execute program
	print "Failed to execute '$cmd': $!\n";
    }
    elsif ($?)
    {
	# Test program failed
	my $sig = $? & 127;
	my $return = $? >> 8;
	print "Command that failed: '$cmd'\n";
	print "Test program killed by signal $sig\n" if $sig;
	print "Test program failed with error $return\n" if $return;

	# Show the last lines of the output 
	my $lines = scalar(@output);
	$lines = $max_lines if $lines > $max_lines;
	print "Last '$lines' lines of output from command:\n";  
	foreach my $line (splice(@output, -$lines))
	{
	    print $line;
	}
    }
    return 0;
}
