# $Id: commit.awk,v 12.0 2004/11/17 03:43:24 bostic Exp $
#
# Output tid of committed transactions.

/txn_regop/ {
	print $5
}
