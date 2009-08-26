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
 * NdbError.java
 */

package ndbjtie;

public class NdbError extends jtie.Wrapper {

    // this c'tor may me protected, for access from JNI is still possible
    protected NdbError(long cdelegate) {
        super(cdelegate);
        System.out.println("<-> ndbjtie.NdbError(long)");
    }

    // while instance methods are convenient to use,
    //public native int code();
    //public native String message();
    // the accurate access model are static methods
    // (field hiding v method overriding)
    //static public native int code(NdbError obj);
    //static public native String message(NdbError obj);
    // however, we can safeguard against overloading using final:
    public native final int code();
    public native final String message();
}
