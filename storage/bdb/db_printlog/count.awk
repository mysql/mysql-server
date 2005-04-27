# $Id: count.awk,v 10.2 1999/11/21 18:01:42 bostic Exp $
#
# Print out the number of log records for transactions that we
# encountered.

/^\[/{
	if ($5 != 0)
		print $5
}
