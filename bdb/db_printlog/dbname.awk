# $Id: dbname.awk,v 1.2 2000/08/03 15:06:39 ubell Exp $
#
# Take a comma-separated list of database names and spit out all the
# log records that affect those databases.

NR == 1 {
	nfiles = 0
	while ((ndx = index(DBNAME, ",")) != 0) {
		filenames[nfiles] = substr(DBNAME, 1, ndx - 1) 0;
		DBNAME = substr(DBNAME, ndx + 1, length(DBNAME) - ndx);
		files[nfiles] = -1
		nfiles++
	}
	filenames[nfiles] = DBNAME 0;
	files[nfiles] = -1
	myfile = -1;
}

/^\[.*log_register/ {
	register = 1;
}
/opcode:/ {
	if (register == 1) {
		if ($2 == 1)
			register = 3;
		else
			register = $2;
	}
}
/name:/ {
	if (register >= 2) {
		for (i = 0; i <= nfiles; i++) {
			if ($2 == filenames[i]) {
				if (register == 2) {
					printme = 0;
					myfile = -1;
					files[i] = -1;
				} else {
					myfile = i;
				}
				break;
			}
		}
	}
	register = 0;
}
/fileid:/{
	if (myfile != -1) {
		files[myfile] = $2;
		printme = 1;
		register = 0;
		myfile = -1;
	} else
		for (i = 0; i <= nfiles; i++)
			if ($2 == files[i]) {
				printme = 1
				break;
			}
}

		

/^\[/{
	if (printme == 1) {
		printf("%s\n", rec);
		printme = 0
	}
	rec = "";

	rec = $0
}
/^	/{
	rec = sprintf("%s\n%s", rec, $0);
}

END {
	if (printme == 1)
		printf("%s\n", rec);
}
