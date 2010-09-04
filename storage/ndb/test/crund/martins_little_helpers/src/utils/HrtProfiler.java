/*
 * HrtStopwatch.java
 */

package utils;

import java.io.PrintWriter;
import java.util.Map;
import java.util.TreeMap;
import java.text.DecimalFormat;

/**
 * A Java High-Resolution Time Method Call Profiling Utility.
 */
public class HrtProfiler {

    static protected final PrintWriter out = new PrintWriter(System.out, true);

    static protected Map< String, Frame > methods
        = new TreeMap< String, Frame >();
    
    static protected class Frame {
        int calls;
        double rtmicros;
        double ctmicros;
    }

    //static public void init(int cap);

    //static public void close();

    static public void enter(String name) {
        HrtStopwatch.pushmark();
    }

    static public void leave(String name) {
        final int t1 = HrtStopwatch.pushmark();
        final int t0 = t1 - 1;
        final double rt = HrtStopwatch.rtmicros(t1, t0);
        final double ct = HrtStopwatch.ctmicros(t1, t0);
        HrtStopwatch.popmark();
        HrtStopwatch.popmark();
        Frame f = methods.get(name);
        if (f == null) {
            f = new Frame();
            Frame g = methods.put(name, f);
            assert (g == null);
        }
        f.calls++;
        f.rtmicros += rt;
        f.ctmicros += ct;
    }

    static public void report() {
        out.println("*** HrtProfile Begin ***");
        out.println("\t#calls \trtmicros \tctmicros \tname");
        for (Map.Entry< String, Frame > e : methods.entrySet()) {
            String m = e.getKey();
            Frame f = e.getValue();
            DecimalFormat nf = new DecimalFormat("###,###,###,###");
            out.print("\t" + nf.format(f.calls));
            out.print("\t" + nf.format(f.rtmicros));
            out.print("\t" + nf.format(f.ctmicros));
            out.println("\t" + m);
        }
        out.println("*** HrtProfile End ***");
    }
    
    //static public void clear();
}
