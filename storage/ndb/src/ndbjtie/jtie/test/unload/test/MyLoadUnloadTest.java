/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * MyLoadUnloadTest.java
 */

package test;

import java.lang.ref.WeakReference;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.io.PrintWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.FileNotFoundException;

import java.net.URL;
import java.net.URI;
import java.net.MalformedURLException;
import java.net.URISyntaxException;
import java.net.URLClassLoader;

import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

/**
 * A test for loading, calling into, and unloading a native library.
 *
 * Under the JNI specification, a native library is loaded by the class loader
 * of that class which called the method System.loadLibrary(String).  In a
 * Java VM, a library cannot be loaded into more than one class loader
 * (since JDK 1.2, to preserve the separation of name-spaces).  A library is
 * unloaded when the class loader containing the library is garbage collected.
 *
 * Therefore, the task of loading of the native library has to be carried
 * out by newly created class loader instances that are separate from the
 * loader of this test class.  This implies that this test class must not
 * statically refer to the code calling into the native library, for this
 * would load the native library by this class loader when running this test,
 * and any subsequent attempt to load the library by another class loader
 * instance would fail (with an UnsatisfiedLinkError).
 *
 * Hence, this class tests the loading and unloading of a native library by
 * executing the following cycle at least twice:
 * <ol>
 * <li> create a test class loader instance,
 * <li> using reflection, instruct the test class loader to load the a
 *      native library and run Java code calling into the native library,
 * <li> clear any (strong) references to the test class loader
 * <li> invoke the system's garbage collector to finalize the test class
 *      loader and unload the library.
 * </ol>
 */
public class MyLoadUnloadTest {

    /**
     *  The stream to write messages to.
     */
    static protected final PrintWriter out = new PrintWriter(System.out, true);

    /**
     *  The stream to write error messages to.
     */
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    // ensure that asserts are enabled
    static {
        boolean assertsEnabled = false;
        assert assertsEnabled = true; // intentional side effect
        if (!assertsEnabled) {
            throw new RuntimeException("Asserts must be enabled for this test to be effective!");
        }
    }
    
    static final String pprefixes_prop
        = "com.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes";
    static final String cname_prop
        = "com.mysql.jtie.test.MyLoadUnloadTest.target_class_name";
    static final String mname_prop
        = "com.mysql.jtie.test.MyLoadUnloadTest.target_method_name";

    /**
     * A weak reference to a class loader loading the native library.
     */
    static protected WeakReference<ClassLoader> cl = null;
    
    /**
     *  Shortcut to the Runtime.
     */
    static private final Runtime rt = Runtime.getRuntime();

    /**
     * A ClassLoader that loads classes from certain packages by itself
     * not delegating loading of these to the parent class loader.
     */
    // this loader is only used for debugging this test
    static class SelfishFileClassLoader extends ClassLoader {

        public SelfishFileClassLoader() {
            out.println("--> SelfishFileClassLoader()");
            out.println("    this = " + this);
            out.println("    getParent() = " + getParent());
            out.println("<-- SelfishFileClassLoader()");
        }
        
        private ByteBuffer getClassData(String name)
            throws ClassNotFoundException {
            out.println("--> SelfishFileClassLoader.getClassData(String)");

            // for simplicity of code, only support loading classes from files;
            // an alternative approach to collect all bytes using
            //   InputStream ClassLoader.getSystemResourceAsStream(String)
            //   ReadableByteChannel Channels.newChannel(InputStream)
            //   int ReadableByteChannel.read(ByteBuffer)
            // requires multiple copying within nested loops, since the total
            // number of bytes to be read is not known in advance.

            String rname = name.replace('.', '/') + ".class";

            // locate the class data
            final URL url = ClassLoader.getSystemResource(rname);
            if (url == null) {
                final String msg = ("no definition of the class "
                                    + rname + " could be found.");
                throw new ClassNotFoundException(msg);
            }
            out.println("    url = " + url);

            // convert into a URI
            final URI uri;
            try {
                uri = url.toURI();
            } catch (URISyntaxException ex) {
                final String msg = ("the URL " + url + " is not formatted"
                                    + "strictly according to RFC2396 and "
                                    + "cannot be converted to a URI.");
                throw new ClassNotFoundException(msg, ex);
            }

            // convert into a pathname
            final File f;
            try {
                f = new File(uri);
            } catch (IllegalArgumentException ex) {
                final String msg = ("the system-dependent URI " + uri
                                    + " cannot be converted into a pathname.");
                throw new ClassNotFoundException(msg, ex);
            }

            // check accessibility
            if (!f.canRead()) {
                final String msg = ("cannot read file " + f);
                throw new ClassNotFoundException(msg);
            }
            out.println("    can read file " + f);
            final long nn = f.length();
            out.println("    f.length = " + nn);

            // open a FileInputStream
            final FileInputStream fis;
            try {
                fis = new FileInputStream(f);
            } catch (FileNotFoundException ex) {
                final String msg = ("the file " + f
                                    + " does not exist, is a directory, or"
                                    + " cannot be opened for reading.");
                throw new ClassNotFoundException(msg, ex);
            }
            
            // get the stream's FileChannel
            final FileChannel fc = fis.getChannel();
            assert (fc != null);
            out.println("    fc = " + fc);

            // allocate a ByteBuffer and read file's content
            final ByteBuffer bb;
            try {
                // not worth mapping file as ByteBuffer
                //   final MappedByteBuffer mbb
                //       = fc.map(MapMode.READ_ONLY, 0, fc.size());
                // unclear performance gain but platform-dependent behaviour

                // retrieve the file's size
                final long n;
                try {
                    n = fc.size();
                } catch (IOException ex) {
                    final String msg = ("cannot get size of file " + f);
                    throw new ClassNotFoundException(msg, ex);
                }
                assert (n == nn);
                
                // allocate a ByteBuffer
                if (n > Integer.MAX_VALUE) {
                    final String msg = ("size of file " + f
                                        + " larger than Integer.MAX_VALUE.");
                    throw new ClassFormatError(msg);
                }
                bb = ByteBuffer.allocateDirect((int)n);

                // read file's content into a ByteBuffer
                try {
                    int k;
                    while ((k = fc.read(bb)) > 0) {
                        out.println("    read " + k + " bytes");
                    }
                } catch (IOException ex) {
                    final String msg = ("cannot read file " + f);
                    throw new ClassNotFoundException(msg, ex);
                }
                assert (bb.remaining() == 0);
                bb.rewind();
            } finally {
                try {
                    fc.close();
                } catch (IOException ex) {
                    final String msg = ("cannot close FileChannel " + fc);
                    throw new ClassNotFoundException(msg, ex);
                }
                try {
                    fis.close();
                } catch (IOException ex) {
                    final String msg = ("cannot close FileInputStream " + fis);
                    throw new ClassNotFoundException(msg, ex);
                }
            }

            out.println("<-- SelfishFileClassLoader.getClassData(String)");
            return bb;
        }

        // Under the Java ClassLoader delegation model, subclasses are
        // encouraged to override findClass(), whose default implementation
        // throws a ClassNotFoundException, rather than loadClass().
        // However, for the purpose of ensuring that certain classes are only
        // loaded by child class loaders, it is not sufficient to override
        // findClass(), which is invoked by loadClass() only after it has
        // delegated loading to the parent class loader with no success.
        protected Class<?> loadClass(String name, boolean resolve)
            throws ClassNotFoundException {
            out.println("--> SelfishFileClassLoader.loadClass(String, boolean)");

            // check if the class has already been loaded
            Class<?> cls = findLoadedClass(name);
            if (cls != null) {
                out.println("    already loaded " + cls);
                return cls;
            }

            //cls = super.findClass(name);
            //cls = super.loadClass(name, resolve);

            // load test's classes by this ClassLoader, delegate others
            if (name.startsWith("test")
                || name.startsWith("myjapi")
                || name.startsWith("jtie")) {
                out.println("    self:    loading " + name + " ...");
                final ByteBuffer bb = getClassData(name);
                cls = defineClass(name, bb, null);
            } else {
                out.println("    parent:  loading " + name + " ...");
                cls = getParent().loadClass(name);
            }
            assert (cls != null);

            out.println("    ... loaded " + cls
                        + " <" + cls.getClassLoader() + ">");

            // link class if requested
            if (resolve) {
                out.println("    linking " + cls + " ...");
                resolveClass(cls);
                out.println("    ... linked " + cls);
            }

            out.println("<-- SelfishFileClassLoader.loadClass(String, boolean)");
            return cls;
        }
    }

    /**
     * A ClassLoader selectively blocking the delegation of the loading of
     * classes to parent class loaders.
     */
    static class FilterClassLoader extends ClassLoader {

        // list of package prefixes not to be loaded by parent class loaders
        protected final String[] prefixes;

        public FilterClassLoader(String[] prefixes) {
            out.println("--> FilterClassLoader()");
            out.println("    this = " + this);
            out.println("    getParent() = " + getParent());
            out.println("    prefixes[] = {");
            for (int i = 0; i < prefixes.length; i++) {
                out.println("        \"" + prefixes[i] + "\"");
            }
            out.println("    }");
            this.prefixes = prefixes;
            out.println("<-- FilterClassLoader()");
        }

        // should never be called, since only invoked by loadClass()
        protected Class<?> findClass(String name)
            throws ClassNotFoundException {
            assert (false) : "should never be called";
            throw new ClassNotFoundException();
        }

        // selectively load classes by parent class loaders 
        protected Class<?> loadClass(String name, boolean resolve)
            throws ClassNotFoundException {
            out.println("--> FilterClassLoader.loadClass(String, boolean)");
            Class<?> cls = null;

            // this loader does not find and load any classes by itself
            assert (findLoadedClass(name) == null);

            // give up on loading if class matches any of the prefixes
            for (int i = prefixes.length - 1; i >= 0; i--) {
                if (name.startsWith(prefixes[i])) {
                    out.println("    redirect loading " + name);
                    out.println("<<< FilterClassLoader.loadClass(String, boolean)");
                    throw new ClassNotFoundException();
                }
            }

            // delegate loading of class to parent class loaders
            out.println("    delegate loading " + name);
            cls = getParent().loadClass(name);
            assert (cls != null);
            //out.println("    ... loaded " + cls
            //            + " <" + cls.getClassLoader() + ">");

            // link class if requested
            if (resolve) {
                out.println("    linking " + cls + " ...");
                resolveClass(cls);
                out.println("    ... linked " + cls);
            }

            out.println("<-- FilterClassLoader.loadClass(String, boolean)");
            return cls;
        }
    }

    /**
     * A URLClassLoader with tracing capabilities for debugging.
     */
    static class MyURLClassLoader extends URLClassLoader {

        public MyURLClassLoader(URL[] urls, ClassLoader parent) {
            super(urls, parent);
            out.println("--> MyURLClassLoader(URL[], ClassLoader)");
            out.println("    this = " + this);
            out.println("    getParent() = " + getParent());
            out.println("    urls[] = {");
            for (int i = 0; i < urls.length; i++) {
                out.println("        " + urls[i]);
            }
            out.println("    }");
            out.println("<-- MyURLClassLoader(URL[], ClassLoader)");
        }

        protected Class<?> findClass(String name)
            throws ClassNotFoundException {
            out.println("--> MyURLClassFinder.findClass(String, boolean)");
            out.println("    finding " + name + " ...");
            Class<?> cls = super.findClass(name);
            out.println("    ... found " + cls
                        + " <" + cls.getClassLoader() + ">");
            out.println("<-- MyURLClassFinder.findClass(String, boolean)");
            return cls;
        }

        protected Class<?> loadClass(String name, boolean resolve)
            throws ClassNotFoundException {
            out.println("--> MyURLClassLoader.loadClass(String, boolean)");
            out.println("    loading " + name + " ...");
            Class<?> cls = super.loadClass(name, resolve);
            out.println("    ... loaded " + cls
                        + " <" + cls.getClassLoader() + ">");
            out.println("<-- MyURLClassLoader.loadClass(String, boolean)");
            return cls;
        }
    }

    /**
     * Attempts to run the JVM's Garbage Collector.
     */
    static private URL[] classPathURLs() throws MalformedURLException {
        final String cp = System.getProperty("java.class.path");
        assert (cp != null) : ("classpath = '" + cp + "'");
        final String[] s = cp.split(File.pathSeparator);
        final URL[] urls = new URL[s.length];
        for (int i = 0; i < s.length; i++) {
            urls[i] = new File(s[i]).toURI().toURL();
        }
        return urls;
    }
    
    static public void test0()
        throws ClassNotFoundException, NoSuchMethodException,
        IllegalAccessException, InvocationTargetException,
        MalformedURLException, InstantiationException {
        out.flush();
        out.println("--> MyLoadUnloadTest.test0()");

        out.println();
        out.println("    MyLoadUnloadTest.class.getClassLoader() = "
                    + MyLoadUnloadTest.class.getClassLoader());
        out.println("    ClassLoader.getSystemClassLoader() = "
                    +  ClassLoader.getSystemClassLoader());

        // read properties specifying the test to run
        final String pprefixes = System.getProperty(pprefixes_prop,
                                                    "test.,myjapi.");
        final String[] pprefix = pprefixes.split(",");
        final String cname = System.getProperty(cname_prop, "test.MyJapiTest");
        final String mname = System.getProperty(mname_prop, "test");
        out.println();
        out.println("    settings:");
        out.println("        pprefixes = '" + pprefixes + "'");
        out.println("        cname = '" + cname + "'");
        out.println("        mname = '" + mname + "'");

        // set up class loaders
        out.println();
        out.println("    create FilterClassLoader ...");
        final ClassLoader fcl = new FilterClassLoader(pprefix);
        out.println("    ... created " + fcl);

        out.println();
        out.println("    create URLClassLoader ...");
        final URL[] urls = classPathURLs();
        //final ClassLoader ucl = new MyURLClassLoader(urls, fcl);
        final ClassLoader ucl = new URLClassLoader(urls, fcl);
        out.println("    ... created " + ucl);

        // store class loader in a global but weak reference
        cl = new WeakReference<ClassLoader>(ucl);

        // run test
        out.println();
        out.println("    load class ...");
        Class<?> cls = ucl.loadClass(cname);
        out.println("    ... loaded " + cls
                    + " <" + cls.getClassLoader() + ">");

        out.println();
        out.println("    get method: " + mname + " ...");
        Method m = cls.getMethod("test");
        out.println("    ... got method: " + m);

        out.println();
        out.println("    create instance: ...");
        Object o = cls.getDeclaredConstructor().newInstance();
        out.println("    ... created instance: " + o);
        
        out.println();
        out.println("    invoke method: " + m + " ...");
        m.invoke(o);
        out.println("    ... invoked method: " + m);
        
        out.println();
        out.println("<-- MyLoadUnloadTest.test0()");
        out.flush();
    }

    /**
     * Attempts to run the JVM's Garbage Collector.
     */
    static private void gc() {
        out.println("--> MyLoadUnloadTest.gc()");
        out.println("    jvm mem: " + (rt.totalMemory() - rt.freeMemory())/1024
                    + " KiB [" + rt.totalMemory()/1024 + " KiB]");
        // empirically determined limit after which no further
        // reduction in memory usage has been observed
        final int nFullGCs = 100;
        //final int nFullGCs = 10;
        for (int i = 0; i < nFullGCs; i++) {
            long oldfree;
            long newfree = rt.freeMemory();
            do {
                // help I/O appear in sync between Java/native
                synchronized (MyLoadUnloadTest.class) {
                    oldfree = newfree;
                    out.println("    rt.gc()");
                    out.flush();
                    err.flush();

                    rt.runFinalization();
                    rt.gc();
                    newfree = rt.freeMemory();
                }
            } while (newfree > oldfree);
        }
        out.println("    jvm mem: " + (rt.totalMemory() - rt.freeMemory())/1024
                    + " KiB [" + rt.totalMemory()/1024 + " KiB]");
        out.println("<-- MyLoadUnloadTest.gc()");
    }

    static public void test()
        throws ClassNotFoundException, NoSuchMethodException,
        IllegalAccessException, InvocationTargetException,
        MalformedURLException, InstantiationException {
        out.flush();
        out.println("--> MyLoadUnloadTest.test()");

        for (int i = 0; i < 3; i++) {
            // create a class loader, load native library, and run test
            test0();

            // garbage collect class loader that loaded native library
            gc();

            // if the class loader with the native library has not been
            // garbage collected, for instance, due to a strong caching
            // reference, the next test invocation will fail with, e.g.:
            //   java.lang.UnsatisfiedLinkError: Native Library <libmyjapi>
            //   already loaded in another classloader
            if (cl.get() != null) {
                out.println("!!! the class loader with the native library"
                            + " has not been garbage collected (for instance,"
                            + " due to a strong caching reference)!");
                break;
            }
        }
        
        out.println();
        out.println("<-- MyLoadUnloadTest.test()");
        out.flush();
    }

    static public void main(String[] args)
        throws Exception {
        out.println("--> MyLoadUnloadTest.main()");

        test();

        // help I/O appear in sync between Java/native
        synchronized (MyLoadUnloadTest.class) {
            //try {
            //    MyLoadUnloadTest.class.wait(1000);
            //} catch (InterruptedException ex) {
            //    // ignore
            //}
            out.flush();
            err.flush();
        }
        
        out.println();
        out.println("<-- MyLoadUnloadTest.main()");
    }
}
