/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * Wrapper.java
 */

package com.mysql.jtie;

/**
 * A base class for Java peer classes representing a C/C++ object type.
 *
 * This class declares only non-public members, which enable the JTie
 * runtime to map a Java </code>Wrapper</code> instance to a C/C++ object.
 * 
 * A Java peer class extending </code>Wrapper</code>
 * <ol>
 * <li> must provide a no-arg constructor (may be of any access modifier,
 *      but preferred private),
 * <li> will declare a native method for each C/C++ (member and non-member)
 *      function to be mapped to Java, and
 * <li> ought not to have any non-static fields (i.e., state by its own).
 * </ol>
 *
 * Please, note that JTie provides no guarantees on the association between
 * </code>Wrapper</code> instances and their underlying C/C++ objects.
 * In particular,
 * <ol>
 * <li> multiple </code>Wrapper</code> instances may exist for the same
 *      C/C++ object at any time and
 * <li> a C/C++ object may have been deleted by the application while
 *      corresponding Java </code>Wrapper</code> instances still exist
 *      (i.e., referential integrity cannot be assumed).
 * </ol>
 */
public class Wrapper {

    /*
     * A native function called during static initialization of this class
     * for preloaded caching of field/method Ids.
     *
     * Caution, this leads easily to circular dependencies: If the underlying
     * native library has not been loaded when this function is called, the
     * result is in a non-descriptive UnsatisfiedLinkError.
     *
     * For this, and since preloading cannot be expected to provide a large
     * performance gain, this feature is not implemented at this time.
     *
     * static private native void initIds();
     * static {
     *     initIds();
     * }
     */

    /**
     * The address of the native delegate object.
     */
    // comments: this field
    // - is allowed to be private, for access from JNI is always possible;
    // - is private for better security and consistency assurances;
    // - must not be final if it is to be nullified when deleting the
    //   native delegate object through this instance from JNI;
    // - is not required to be final under the hashcode() and equals()
    //   consistency requirement;
    // - XXX consider (and benchmark) declaring this field volatile
    //   - a write then happens-before every subsequent read of that field
    //   - writes and reads of volatile longs and doubles are always atomic
    private volatile long cdelegate;

    /**
     * Creates an unattached wrapper instance.
     */
    // comments:
    // - field access from JNI is fast, hence no initialization of this
    //   cdelegate field in a c'tor;
    protected Wrapper() {
        //System.out.println("<-> jtie.Wrapper()");
    };

    /**
     * Copies a wrapper instance.
     */
    protected Wrapper(Wrapper o) {
        //System.out.println("<-> jtie.Wrapper(Wrapper)");
        cdelegate = o.cdelegate;
    };

    /**
     * Indicates whether some other object is a wrapper that refers to the
     * same native delegate instance.
     * @see Object#equals(Object)
     */
    // all equals() requirements are met due to pure delegation semantics
    public final boolean equals(Object obj) {
        if (this == obj)
            return true;
        if (!(obj instanceof Wrapper))
            return false;
        final Wrapper wo = (Wrapper)obj;
        return (cdelegate == wo.cdelegate);
    }

    /**
     * Returns a hash code value for the object.
     * @see Object#hashCode()
     */
    // all hashCode() requirements are met due to pure delegation semantics
    public final int hashCode() {
        return (int)cdelegate; // precision loss ok, for only a hash
    }

    /**
     * Returns a string representation of the object.
     * @see Object#toString()
     */
    // overrides inherited toString() for full precision of cdelegate
    public String toString() {
        return (getClass().getName() + '@' + Long.toHexString(cdelegate));
    }
}
