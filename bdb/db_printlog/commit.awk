# $Id: commit.awk,v 10.2 1999/11/21 18:01:42 bostic Exp $
#
# Output tid of committed transactions.

/txn_regop/ {
	print $5
}
