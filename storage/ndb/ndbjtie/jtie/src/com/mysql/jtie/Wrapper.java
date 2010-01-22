/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

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
/*
 * Wrapper.java
 */

package com.mysql.jtie;

/**
 * The JTIE base class which all application classes must extend that map
 * to an underlying C/C++ type.
 * 
 * The other requirement for subclasses are
 * <ol>
 * <li> to provide a no-arg constructor (of any access modifier, since
 * JNI can access even private members).
 * <li> to declare the native methods to be mapped to C/C++ functions.
 * </ol>
 * It is not useful for subclasses of </code>Wrapper</code> to have state
 * of their own (that is, beyond the field of this class).
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
    private /*volatile*/ long cdelegate;

    /**
     * Creates an unattached Wrapper instance.
     */
    // comments:
    // - field access from JNI is fast, hence no initialization of this
    //   cdelegate field in a c'tor;
    // - the constructor needs to be protected for subclasses.
    protected Wrapper() {
        //System.out.println("<-> jtie.Wrapper()");
    };

    /**
     * Indicates whether some other object is a Wrapper that refers to the
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
