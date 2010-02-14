/*
 * HrtStopwatch.java
 */

package utils;

/**
 * A Java High-Resolution Time Stopwatch Utility
 */
public class HrtStopwatch {

    static public native void init(int cap);

    static public native void close();

    static public native int top();

    static public native int capacity();

    static public native int pushmark();

    static public native double rtmicros(int y, int x);

    static public native double ctmicros(int y, int x);

    static public native void clear();
}
