// $Id$
/** This is a simple java program that tests the querying feature of the
    JNI interface.  It approximately replicates the functionality of
    ../example/tcapi.c.

   The basic command line options are
   @code
   datadir selection-conditions [<column type> <column type>...]
   @endcode

   Types recognized are: i (for integers), l (for long integers), f (for
   floats) and d for (doubles).  Unrecognized types are treated as
   integers.

   @note Milky Stork (Mycteria cinerea)
   A endangered white stork that was common in Java, Indonesia.
   @see http://www.birdlife.org/datazone/sites/index.html?action=SpcHTMDetails.asp&sid=3825

   @author John Wu
 */
import java.io.*;

public class milky {
    public static void main(String args[]) {
	int exitCode, iarg, msglvl;
	String conffile;

	iarg = 0;
	msglvl = 0;
	conffile = "";
	while (iarg < args.length) {
	    if (args[iarg].charAt(0) != '-') break;
	    if (args[iarg].charAt(1) == 'c' || args[iarg].charAt(1) == 'C') {
		if (iarg+1 < args.length) {
		    conffile = args[iarg+1];
		    iarg += 2;
		}
		else {
		    iarg += 1;
		}
	    }
	    else if (args[iarg].charAt(1) == 'h' ||
		     args[iarg].charAt(1) == 'H') {
		usage();
		++ iarg;
	    }
	    else if (args[iarg].charAt(1) == 'm' ||
		     args[iarg].charAt(1) == 'M' ||
		     args[iarg].charAt(1) == 'v' ||
		     args[iarg].charAt(1) == 'V') {
		if (iarg+1 < args.length &&
		    (Character.isDigit(args[iarg+1].charAt(0)) ||
		     (Character.isDigit(args[iarg+1].charAt(1)) &&
		      args[iarg+1].charAt(0) == '-'))) {
		    msglvl += Integer.parseInt(args[iarg+1]);
		    iarg += 2;
		}
		else {
		    msglvl += 1;
		    iarg += 1;
		}
	    }
	    else {
		System.out.println("** unknown option " + args[iarg]);
		iarg += 1;
	    }
	}

	gov.lbl.fastbit.FastBit fb = new gov.lbl.fastbit.FastBit(conffile);
	exitCode = fb.set_message_level(msglvl);
	if (msglvl > 1)
	    System.out.println("FastBit.set_messsage_level(" + msglvl
			       + ") returned " + exitCode);

	if (args.length > iarg) {
	    String cmds[] = new String[args.length-iarg];
	    for (int j = 0; j < args.length-iarg; ++ j)
		cmds[j] = args[iarg+j];
	    traverseDir(fb, cmds);
	}
	else {
	    builtin(fb);
	}
    }

    private static void usage() {
	System.out.println
	    ("A simple tester for the Java API of FastBit\n\nusage\n" +
	     "java milky [-c conffile] [-v [message-level]] " +
	     "datadir [conditions] [<column-name type> ...]\n" +
	     "In SQL this is equivalent to\n\tFROM datadir " +
	     "[WHERE conditions [SELECT column-name ...]]\n\n" +
	     "If only datadir is present, milky indexes all columns in " +
	     "the named directory.\n" +
	     "If conditions are provided without columns to print, " +
	     "milky will print the number of hits.\n" +
	     "If any variable is to be printed, it must be specified as " +
	     "a <name type> pair.\n\n" +
	     "Example:\n" +
	     "java milky dir 'c1 = 15 and c2 > 23' c1 i c3 d\n");
    }

    private static void builtin(gov.lbl.fastbit.FastBit fb) {
	int nerrors = 0;
	int mult;
	int msglvl = fb.get_message_level();
	String dir = "tmp";
	int[] counts = {5, 24, 19, 10, 50};
	String[] conditions =
	    {"a<5", "a+b>150", "a < 60 and c < 60", "c > 90", "c > a"};
	int[] ivals = new int[100];
	short[] svals = new short[100];
	float[] fvals = new float[100];

	System.out.println("milky -- starting built-in tests ...");
	// prepare a sample data
	for (int i = 0; i < 100; ++ i) {
	    ivals[i] = i;
	    svals[i] = (short) i;
	    fvals[i] = (float) (1e2 - i);
	}
	fb.add_ints("a", ivals);
	fb.add_shorts("b", svals);
	fb.add_floats("c", fvals);
	fb.write_buffer(dir);
	// test the queries
	mult = fb.number_of_rows(dir);
	if (mult % 100 != 0) { /* no an exact multiple */
	    System.out.println("Directory " + dir + " contains " + mult +
			       " rows, but expected 100, remove the " +
			       "directory and try again");
	    return;
	}

	mult /= 100;
	for (int i = 0; i < counts.length; ++ i) {
	    gov.lbl.fastbit.FastBit.QueryHandle h =
		fb.build_query(null, dir, conditions[i]);
	    int nhits = fb.get_result_size(h);
	    if (nhits != mult * counts[i]) {
		++ nerrors;
		System.out.println("query \"" + conditions[i] + "\" on " +
				   mult*100 + " build-in records found " +
				   nhits + " hits, but " + mult*counts[i] +
				   " were expected");
	    }
            else if (msglvl > 1) {
                int[] rowIds= fb.get_result_row_ids(h);
                if (rowIds!=null) {
                    System.out.println("START row ids");
                    for (int j = 0; j < rowIds.length; ++j) {
                        System.out.println(rowIds[j]);
                    }
                    System.out.println("END row ids");
                }
            }
	    fb.destroy_query(h);
	}
	System.out.println("milky -- built-in tests finished with nerrors = " +
			   nerrors);
    }

    /** Traverse the specified directory recursively with java.io.File
     * class.
     */
    private static int traverseDir(gov.lbl.fastbit.FastBit fb, String cmds[]) {
	int ret = 0;
	if (cmds.length == 0) return ret;
	File fd = new java.io.File(cmds[0]);
	Boolean hasparttxt = false;
	String[] children = fd.list();
	if (children == null) return ret;
	if (children.length == 0) return ret;
	for (int j = 0; j < children.length; ++ j) {
	    String dname = fd.getPath();
	    dname += '/';
	    dname += children[j];
	    if (children[j].compareTo("-part.txt") != 0) {
		cmds[0] = dname;
		int ierr = traverseDir(fb, cmds);
		if (ierr > 0) {
		    ret += ierr;
		}
		else if (ierr < 0) {
		    System.out.println("traverseDir(" + dname +
				       ") failed with error code " + ierr);
		}
	    }
	    else {
		hasparttxt = true;
	    }
	}
	if (hasparttxt) {
	    cmds[0] = fd.getPath();
	    int ierr = processQuery(fb, cmds);
	    if (ierr > 0) {
		++ ret;
	    }
	    else if (ierr < 0) {
		System.out.println("processQuery(" + cmds[0] +
				   ") failed with error code " + ierr);
	    }
	}
	return ret;
    }

    /**  Process the user specified query.
     */
    private static int processQuery(gov.lbl.fastbit.FastBit fb, String cmds[]) {
	if (cmds.length == 0) return 0;
	int ierr, nhits, nprt;
	long starttime=0;
	int iarg = 0;
	int msglvl = fb.get_message_level();
	if (msglvl > 0) {
	    System.out.println("\nstarting to process directory " + cmds[0]);
	    starttime = System.currentTimeMillis();
	}
	if (cmds.length == iarg+1) { // builting index only
	    ierr = fb.build_indexes(cmds[iarg], "");
	    if (msglvl >= 0)
		System.out.println("FastBit.build_indexes returned "
				   + ierr);
	    return (ierr < 0 ? ierr : 1);
	}

	gov.lbl.fastbit.FastBit.QueryHandle h =
	    fb.build_query(null, cmds[iarg], cmds[iarg+1]);
	if (h == null) {
	    System.out.println("failed to process query \"" + cmds[iarg+1]
			       + "\" on data in " + cmds[iarg]);
	    return -1;
	}

	nhits = fb.get_result_size(h);
	System.out.println("\napplying \"" + cmds[iarg+1] + "\" on data in "
			   + cmds[iarg] + " produced " + nhits + " hit"
			   + (nhits>1?"s":""));
	nprt = (msglvl > 30 ? nhits :
		msglvl > 0 ? (1 << msglvl) : -1);
	if (nprt+nprt > nhits)
	    nprt = nhits;
        if (msglvl > 1) {
            int[] rowIds= fb.get_result_row_ids(h);
            if (rowIds!=null) {
                System.out.println("START row ids");
                for (int i = 0; i < rowIds.length; i++) {
                    System.out.println(rowIds[i]);
                }
                System.out.println("END row ids");
            }
        }

	iarg += 2;
	for (int i = iarg; i < cmds.length; i += 2) {
	    char t = (i+1 < cmds.length ? cmds[i+1].charAt(0) : 'i');
	    switch (t) {
	    default:
	    case 'i':
	    case 'I': {
		int tmp[] = fb.get_qualified_ints(h, cmds[i]);
		if (tmp != null) {
		    if (nprt > 0) {
			System.out.print(cmds[i] + "[" + nhits + "]:");
			for (int j = 0; j < nprt; ++ j)
			    System.out.print(" " + tmp[j]);
			if (nprt < nhits)
			    System.out.print(" ... (" + (nhits-nprt) +
					     " skipped)");
			System.out.println();
		    }
		}
		else {
		    System.out.println("** failed to retrieve values for "
				       + "column " + cmds[i]
				       + " (requested type i)");
		}
		break;}
	    case 'l':
	    case 'L': {
		long tmp[] = fb.get_qualified_longs(h, cmds[i]);
		if (tmp != null) {
		    if (nprt > 0) {
			System.out.print(cmds[i] + "[" + nhits + "]:");
			for (int j = 0; j < nprt; ++ j)
			    System.out.print(" " + tmp[j]);
			if (nprt < nhits)
			    System.out.print(" ... (" + (nhits-nprt) +
					     " skipped)");
			System.out.println();
		    }
		}
		else {
		    System.out.println("** failed to retrieve values for "
				       + "column " + cmds[i]
				       + " (requested type i)");
		}
		break;}
	    case 'f':
	    case 'F':
	    case 'r':
	    case 'R': {
		float tmp[] = fb.get_qualified_floats(h, cmds[i]);
		if (tmp != null) {
		    if (nprt > 0) {
			System.out.print(cmds[i] + "[" + nhits + "]:");
			for (int j = 0; j < nprt; ++ j)
			    System.out.print(" " + tmp[j]);
			if (nprt < nhits)
			    System.out.print(" ... (" + (nhits-nprt) +
					     " skipped)");
			System.out.println();
		    }
		}
		else {
		    System.out.println("** failed to retrieve values for "
				       + "column " + cmds[i]
				       + " (requested type f)");
		}
		break;}
	    case 'd':
	    case 'D': {
		double tmp[] = fb.get_qualified_doubles(h, cmds[i]);
		if (tmp != null) {
		    if (nprt > 0) {
			System.out.print(cmds[i] + "[" + nhits + "]:");
			for (int j = 0; j < nprt; ++ j)
			    System.out.print(" " + tmp[j]);
			if (nprt < nhits)
			    System.out.print(" ... (" + (nhits-nprt) +
					     " skipped)");
			System.out.println();
		    }
		}
		else {
		    System.out.println("** failed to retrieve values for "
				       + "column " + cmds[i]
				       + " (requested type d)");
		}
		break;}
	    }
	}

	ierr = fb.destroy_query(h);
	if (msglvl > 0) {
	    System.out.println("FastBit.destroy_query returned " + ierr);
	    System.out.println("Processing directory " + cmds[0] + " took " +
			       (System.currentTimeMillis() - starttime) + " ms");
	}
	return 1;
    }
} // milky
