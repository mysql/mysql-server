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
 * B0.java
 */

package myjapi;

// ---------------------------------------------------------------------------
// generatable, user-API-dependent Java wrapper class
// ---------------------------------------------------------------------------

public class B0 extends jtie.Wrapper {

    // with default constructor, cdelegate needs to be written from JNI
    //protected B0() {
    //    System.out.println("<-> myjapi.B0()");
    //};

    // this c'tor may me protected, for access from JNI is still possible
    protected B0(long cdelegate) {
        super(cdelegate);
        //System.out.println("<-> myjapi.B0(" + Long.toHexString(cdelegate) + ")");
    };

    // static method
    static public native int f0s();

    // non-virtual method
    static public native int f0n(B0 p0);
        
    // virtual method
    public native int f0v();
}
