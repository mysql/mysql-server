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
 * A.java
 */

package demoj;

// ---------------------------------------------------------------------------
// generatable, user-API-dependent Java wrapper class
// ---------------------------------------------------------------------------

public class A {

    // the address of the C delegate object
    // this field may me private, access from JNI is still possible
    private /*final*/ long cdelegate;

    // this c'tor may me protected, access from JNI is still possible
    protected A(long cdelegate) {
        this.cdelegate = cdelegate;
        System.out.println("<-> demoj.A(" + Long.toHexString(cdelegate) + ")");
    };

    // delegates to: static double A::simple(double p0)
    static public native double simple(double p0);
    
    // delegates to: static void A::print(String p0)
    static public native void print(String p0);
    
    // delegates to: static A A::getA();
    static public native A getA();

    // delegates to: void A::print()
    public native void print();

    // equals semantics is defined over cdelegate value only
    public boolean equals(Object obj) {
        if (this == obj)
            return true;
        if (!(obj instanceof A))
            return false;
        final A wo = (A)obj;
        return (cdelegate == wo.cdelegate);
    }
    
    // hashcode semantics is defined over cdelegate value only
    public int hashCode() {
        // ok to loose precision, since only a hash
        return (int)cdelegate;
    }

    // string representation is defined over cdelegate value only
    public String toString() {
        return (getClass().getName() + "@" + Long.toHexString(cdelegate));
    }
}
