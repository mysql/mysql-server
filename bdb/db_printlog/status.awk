# $Id: status.awk,v 10.2 1999/11/21 18:01:43 bostic Exp $
#
# Read through db_printlog output and list all the transactions encountered
# and whether they commited or aborted.
#
# 1 = started
# 2 = commited
BEGIN {
	cur_txn = 0
}
/^\[/{
	if (status[$5] == 0) {
		status[$5] = 1;
		txns[cur_txn] = $5;
		cur_txn++;
	}
}
/txn_regop/ {
	status[$5] = 2
}
END {
	for (i = 0; i < cur_txn; i++) {
		printf("%s\t%s\n",
		    txns[i], status[txns[i]] == 1 ? "ABORT" : "COMMIT");
	}
}
