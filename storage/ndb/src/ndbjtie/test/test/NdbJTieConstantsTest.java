/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
 * NdbJTieConstantsTest.java
 */

package test;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;

import com.mysql.ndbjtie.ndbapi.NDBAPI;

/**
 * Verifies the constants in NdbJTie against their values in NDB API.
 */
public class NdbJTieConstantsTest extends JTieTestBase {

    static protected native long nativeConstValue(String constName);

    static protected final String ndbjtie_jar_file_prop
        = "test.NdbJTieConstantsTest.ndbjtie_jar_file";

    static protected final long unknown = 0x0abcdef00fedcba0L;

    protected final List<String> classNames = new ArrayList<String>();

    protected final List<String> errors = new ArrayList<String>();
    protected int nFields;
    protected int nClasses;
    protected int nPackages;

    protected void fetchClassNames(String jarFile) {
        try {
            final JarInputStream jStream
                = new JarInputStream(new FileInputStream(jarFile));
            try {
                JarEntry e;
                while ((e = jStream.getNextJarEntry()) != null) {
                    final String en = e.getName();
                    if (en.endsWith(".class")) {
                        String cn = en.replace("/", ".").replace("\\", ".");
                        cn = cn.substring(0, cn.length() - ".class".length());
                        classNames.add(cn);
                    }
                    jStream.closeEntry();
                }
            } finally {
                jStream.close();
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public NdbJTieConstantsTest() {
        out.println("--> NdbJTieConstantsTest()");

        // read properties
        final String jar_file
            = System.getProperty(ndbjtie_jar_file_prop, "ndbjtie.jar");
        out.println("    settings:");
        out.println("        ndbjtie_jar_file = '" + jar_file + "'");

        // read classes
        out.println("    parsing jar file ...");
        fetchClassNames(jar_file);
        out.println("    ... found " + classNames.size() + " classes");

        out.println("<-- NdbJTieConstantsTest()");
    }
    
    public void clear() {
        errors.clear();
        nFields = 0;
        nClasses = 0;
        nPackages = 0;
    }

    public void validateField(Field f) {
        nFields++;
        out.println("            " + f.getName());
        final int m = f.getModifiers();
        assert Modifier.isStatic(m);
        assert Modifier.isPublic(m);
        assert Modifier.isFinal(m);

        final String name = (f.getDeclaringClass().getName()
                             + "." + f.getName());
        final long expected = nativeConstValue(name);
        if (expected == unknown) {
            errors.add("unknown constant '" + name + "'");
            return;
        }

        final long actual;
        try {
            actual = f.getLong(null);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
        if (expected != actual) {
            errors.add("numeric value mismatch for constant '" + name + "'"
                       + "; expected = " + expected + "; actual = " + actual);
        }
    }
    
    public void validateClass(Class<?> c) {
        nClasses++;
        out.println("        " + c.getName());
        for (Field f : c.getDeclaredFields()) {
            validateField(f);
        }
    }

    public void validatePackage(String pkg) {
        nPackages++;
        out.println("    validating: " + pkg);
        try {
            for (String name : classNames) {
                if (name.startsWith(pkg))
                    validateClass(Class.forName(name));
            }
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    public void test() {
        out.println("--> NdbJTieConstantsTest.test()");
        clear();

        // load native library
        out.println();
        loadSystemLibrary("ndbjtie_unit_tests");

        // test unknown constant
        out.println();
        final String n = "_an_unknown_c0nstant_";
        out.println("    validating: " + n);
        final long l = nativeConstValue(n);
        assert l == 0x0abcdef00fedcba0L;

        // test classes in package
        out.println();
        validatePackage("com.mysql.ndbjtie.ndbapi.");

        // report status
        out.println();
        out.println("    TEST STATUS:");
        for (String s : errors)
            out.println("    !!! " + s);
        out.println("    #packages:  " + nPackages);
        out.println("    #classes:   " + nClasses);
        out.println("    #fields:    " + nFields);
        out.println("    #errors:    " + errors.size());
        assert errors.isEmpty() : "data errors in test";

        out.println();
        out.println("<-- NdbJTieConstantsTest.test()");
    };

    static public void usage() {
        out.println("usage:");
        out.println("java -cp <ndbjtie-test.jar>:<ndbjtie.jar>");
        out.println("    -D" + ndbjtie_jar_file_prop + "=<ndbjtie.jar>");
        out.println("    test.NdbJTieConstantsTest");
        System.exit(1);
    }

    static public void main(String[] args) throws Exception {
        out.println("--> NdbJTieConstantsTest.main()");

        if (args.length != 0)
            usage();

        out.println();
        NdbJTieConstantsTest test = new NdbJTieConstantsTest();
        test.test();

        out.println();
        out.println("<-- NdbJTieConstantsTest.main()");
    }
}
