# $Id: count.awk,v 12.0 2004/11/17 03:43:24 bostic Exp $
#
# Print out the number of log records for transactions that we
# encountered.

/^\[/{
	if ($5 != 0)
		print $5
}
