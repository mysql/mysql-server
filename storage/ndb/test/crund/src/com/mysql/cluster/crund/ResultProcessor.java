/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.cluster.crund;

import java.util.Properties;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.TreeMap;
import java.util.EnumSet;
import java.text.ParseException;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.InputStream;


/**
 * Processes the result files of the Crund benchmark.
 */
public class ResultProcessor {
    // console
    static protected final PrintWriter out = new PrintWriter(System.out, true);
    static protected final PrintWriter err = new PrintWriter(System.err, true);
    static protected final String endl = System.getProperty("line.separator");

    // command-line arguments
    static private final List<String> ilogFileNames
        = new ArrayList<String>();
    static private enum OutputFormat { txt, csv };
    static private final EnumSet<OutputFormat> outputFormats
        = EnumSet.noneOf(OutputFormat.class);
    static private int nWarmupRuns;
    static private int largeRSDev;
    static private String outFilePrefix;

    // resources
    protected List<Reporter> reporters = new ArrayList<Reporter>();
    protected List<Results> results = new ArrayList<Results>();
    protected Results current;

    public class Results {
        public String[] header;
        public long nRows;
        public long nRuns;
        public double[] ravg;
        public double[] rdev;
    }

    // ----------------------------------------------------------------------
    // Reporting
    // ----------------------------------------------------------------------

    /**
     * Reports the results of the Crund benchmark.
     */
    public abstract class Reporter {
        protected final String suffix;
        protected File tmp;
        protected PrintWriter dst;

        public Reporter(String suffix) {
            this.suffix = suffix;
        }

        /**
         * Initializes this reporter.
         */
        public void init() throws IOException {
            assert tmp == null;
            assert dst == null;
            tmp = File.createTempFile("crund_log_results", suffix);
            dst = new PrintWriter(tmp);
        }
        
        /**
         * Releases resources held by this reporter.
         */
        public void close() throws IOException {
            assert tmp != null;
            assert dst != null;
            dst.flush();
            final File to = new File(outFilePrefix + "." + suffix);
            if (tmp.renameTo(to)) {
                out.println("wrote file: \t" + to);
            } else {
                out.flush();
                err.println("ERROR: failed to rename temp file:" + endl
                            + "    '" + tmp + "'" + endl
                            + " to output file:" + endl
                            + "    '" + to + "'");
            }
            tmp = null;
            dst.close();
            dst = null;
        }

        /**
         * Reports a data series.
         *
         * @param       tag     a name for this data series
         * @param       op      the names of the operations
         * @param       avg     the average values
         * @param       sdev    the standard deviations
         * @param       rsdev   the relative standard deviations
         */
        abstract void report(String tag,
                             long nRuns,
                             long nRows,
                             String[] op,
                             double[] avg,
                             double[] sdev,
                             double[] rsdev);
    }

    public class TxtReporter extends Reporter {
        public TxtReporter() {
            super("txt");
        }

        public void report(String tag,
                           long nRuns,
                           long nRows,
                           String[] op,
                           double[] avg,
                           double[] sdev,
                           double[] rsdev) {
            dst.println();
            dst.println("------------------------------------------------------------");
            dst.println("ops: " + tag + " " + nRows + " rows"
                        + " (using last " + nRuns + " runs)");
            dst.println("------------------------------------------------------------");

            // ops with large deviations
            final Map<Double,String> ldev = new TreeMap<Double,String>();

            dst.println();
            dst.println(String.format("%-24s", "op")
                        + String.format("%12s", "#rows/metric")
                        + String.format("%12s", "metric/row")
                        + String.format("%12s", "rsdev%"));
            for (int i = 0; i < op.length; i++) {
                final double a = avg[i];
                final double opa = (a > 0 ? nRows / a : -1);
                final double apo = (nRows > 0 ? a / nRows : -1);
                dst.println(String.format("%-24s", op[i])
                            + String.format("%12.2f", opa)
                            + String.format("%12.2f", apo)
                            + String.format("%12.2f", rsdev[i]));
                if (rsdev[i] > largeRSDev)
                    ldev.put(-rsdev[i], op[i]);
            }

            if (ldev.size() > 1) {
                dst.println();
                dst.println(String.format("%-24s", "ops with rel. deviations")
                            + String.format("%12s", "> " + largeRSDev + "%"));
                for (Map.Entry<Double,String> e : ldev.entrySet()) {
                    dst.println(
                        String.format("%-24s", e.getValue())
                        + String.format("%12.2f", -e.getKey()));
                }
            }
        }
    }

    public class CsvReporter extends Reporter {
        public CsvReporter() {
            super("csv");
        }

        public void report(String tag,
                           long nRuns,
                           long nRows,
                           String[] op,
                           double[] avg,
                           double[] sdev,
                           double[] rsdev) {
            dst.println("metric,load,#rows,#runs,op,xmode,"
                        + "#rows/metric,metric/row,rsdev");
            if (nRuns > 0) {
                for (int i = 0; i < op.length; i++) {
                    String ml = tag;   // =metric,load
                    if (!ml.contains(","))
                        ml = ml + ",null";
                    String om = op[i]; // =op,xmode
                    if (!om.contains(","))
                        om = om + ",null";
                    final double a = avg[i];
                    final double rpa = (a > 0 ? nRows / a : -1);
                    final double apr = (nRows > 0 ? a / nRows : -1);
                    dst.println(ml
                                + "," + nRows
                                + "," + nRuns
                                + "," + om
                                + "," + String.format("%.2f", rpa)
                                + "," + String.format("%.2f", apr)
                                + "," + String.format("%.2f", rsdev[i]));
                }
            }
        }
    }

    // ----------------------------------------------------------------------
    // Processing
    // ----------------------------------------------------------------------

    /**
     * Runs the result processor.
     */
    public void run() {
        try {
            init();

            out.println();
            for (String fn : ilogFileNames) {
                out.println("parse file: \t" + fn);
                BufferedReader ilog = null;
                try {
                    ilog = new BufferedReader(new FileReader(fn));
                    parse(ilog);
                } finally {
                    if (ilog != null)
                        ilog.close();
                }
            }

            report();
            close();
        } catch (Exception ex) {
            // end the program regardless of threads
            out.println("caught " + ex);
            ex.printStackTrace();
            System.exit(2); // return an error code
        }
    }

    protected void parse(BufferedReader ilog)
        throws IOException, ParseException {

        long lineNo = 1;
        long nRuns = 0;
        String[] header = null;
        while (true) {
            final String line = ilog.readLine();
            if (line == null)
                return; // eof
            lineNo++;
            //out.println(line);

            // skip comments
            if (line.startsWith("#"))
                continue;

            // parse empty line
            if (line.equals("")) {
                header = null;
                continue;
            }

            // parse header line
            if (header == null) {
                header = line.split("\\t");
                process(header, lineNo);
                nRuns = 0;
                continue;
            }

            // skip warmup runs
            if (nRuns++ < nWarmupRuns)
                continue;

            // parse value line
            final String[] values = line.split("\\t");
            if (values.length != header.length) {
                final String msg = ("line # " + lineNo
                                    + ": wrong number of tokens; "
                                    + "expected: " + header.length
                                    + ", found: " + values.length);
                throw new ParseException(msg, 0);
            }
        
            // parse values
            long[] val = new long[values.length];
            for (int i = 0; i < values.length; i++) {
                try {
                    val[i] = Long.valueOf(values[i]);
                } catch (NumberFormatException e) {
                    final String msg = ("line # " + lineNo + ": " + e);
                    throw new ParseException(msg, i);
                }
            }
            process(val, lineNo);
        }
    }

    protected void process(String[] header, long lineNo)
        throws ParseException {
        assert 0 < header.length;
        current = new Results();
        current.header = header;
        //nRuns = 0;
        //nRows = 0;
        current.ravg = new double[header.length];
        current.rdev = new double[header.length];
        results.add(current);
    }

    protected void process(long[] values, long lineNo)
        throws ParseException {
        assert 0 < values.length;
        assert values.length == current.header.length;

        // check first nRows count
        final long nRows = values[0];
        if (current.nRuns++ == 0) {
            current.nRows = nRows;
        } else if (nRows != current.nRows) {
            final String msg = ("line # " + lineNo
                                + ": unexpected nRows; "
                                + "expected: " + current.nRows
                                + ", found: " + nRows);
            throw new ParseException(msg, 0);
        }

        // compute running averages and squared deviations
        for (int i = 1; i < values.length; i++) {
            final double v = values[i];
            final double oavg = current.ravg[i];
            final double navg = oavg + (v - oavg) / current.nRuns;
            final double odev = current.rdev[i];
            final double ndev = odev + (v - oavg) * (v - navg);
            current.ravg[i] = navg;
            current.rdev[i] = ndev;
        }
    }

    protected void report() {
        for (Results r : results) {
            final int nops = r.header.length - 1;
            final String[] op = new String[nops];
            final double[] avg = new double[nops];
            final double[] sdev = new double[nops];
            final double[] rsdev = new double[nops];
            
            // compute results
            final String tag = r.header[0];
            if (r.nRuns > 0) {
                for (int i = 1; i <= nops; i++) {
                    op[i-1] = r.header[i];
                    avg[i-1] = r.ravg[i];
                    sdev[i-1] = Math.sqrt(r.rdev[i] / r.nRuns);
                    rsdev[i-1] = (sdev[i-1] * 100.0) / avg[i-1];
                }
            }
            
            for (Reporter rp : reporters)
                rp.report(tag, r.nRuns, r.nRows, op, avg, sdev, rsdev);
        }
    }

    // ----------------------------------------------------------------------
    // intializers/finalizers
    // ----------------------------------------------------------------------

    // initializes the result processor's resources.
    protected void init() throws Exception {
        for (OutputFormat f : outputFormats) {
            switch (f) {
            case txt : 
                reporters.add(new TxtReporter());
                break;
            case csv: 
                reporters.add(new CsvReporter());
                break;
            }
        }
        for (Reporter r : reporters)
            r.init();
    }

    // releases the result processor's resources.
    protected void close() throws Exception {
        for (Reporter r : reporters)
            r.close();
        reporters.clear();
    }

    // prints the result processor's properties
    protected void printProperties() {
        out.println("settings ...");
        out.println("nWarmupRuns:                    " + nWarmupRuns);
    }

    // ----------------------------------------------------------------------
    // usage
    // ----------------------------------------------------------------------

    // prints command-line usage and exits
    static protected void exitUsage() {
        err.println("usage: [options] <log file>...");
        err.println("    [-w <number>]       skip w warmup runs [w=0]");
        err.println("    [-d <number>]       mark ops with rsdev>d [d=10]");
        err.println("    [-f txt,csv]        generate output formats [txt]");
        err.println("    [-o <file>]         out file prefix [./log_results]");
        err.println("    [-h|--help]         print usage message and exit");
        err.println();
        System.exit(1); // return an error code
    }

    // parses command-line arguments
    static public void parseArguments(String[] args) {
        String w = "0";
        String d = "10";
        String f = "txt";
        String o = "./log_results";
        for (int i = 0; i < args.length; i++) {
            final String arg = args[i];
            if (arg.equals("-w")) {
                if (i >= args.length)
                    exitUsage();
                w = args[++i];
            } else if (arg.equals("-d")) {
                if (i >= args.length)
                    exitUsage();
                d = args[++i];
            } else if (arg.equals("-f")) {
                if (i >= args.length)
                    exitUsage();
                f = args[++i];
            } else if (arg.equals("-o")) {
                if (i >= args.length)
                    exitUsage();
                o = args[++i];
            } else if (arg.equals("-h") || arg.equals("--help")) {
                exitUsage();
            } else if (arg.startsWith("-")) {
                out.println("unknown option: " + arg);
                exitUsage();
            } else {
                ilogFileNames.add(args[i]);
            }
        }

        try {
            if ((nWarmupRuns = Integer.valueOf(w)) < 0) {
                err.println("illegal value: -w '" + w + "'");
                exitUsage();
            }
        } catch (NumberFormatException e) {
            err.println("not a number: -w '" + w + "'");
            exitUsage();
        }
        try {
            if ((largeRSDev = Integer.valueOf(d)) < 0) {
                err.println("illegal value: -d '" + d + "'");
                exitUsage();
            }
        } catch (NumberFormatException e) {
            err.println("not a number: -d '" + d + "'");
            exitUsage();
        }

        try {
            for (String s : f.split(","))
                outputFormats.add(OutputFormat.valueOf(s));
        } catch (IllegalArgumentException e) {
            err.println("illegal value: -f '" + f + "'");
            exitUsage();            
        }
        if ((outFilePrefix = o).isEmpty()) {
            err.println("illegal file prefix: -o '" + o + "'");
            exitUsage();
        }
        if (ilogFileNames.size() == 0) {
            err.println("missing input files");
            exitUsage();
        }
    }

    static public void main(String[] args) {
        System.out.println("ResultProcessor.main()");
        parseArguments(args);
        new ResultProcessor().run();
        System.out.println();
        System.out.println("ResultProcessor.main(): done.");
    }
}
