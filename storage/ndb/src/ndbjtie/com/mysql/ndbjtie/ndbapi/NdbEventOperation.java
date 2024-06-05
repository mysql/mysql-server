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
 * NdbEventOperation.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbEventOperation extends Wrapper implements NdbEventOperationConst
{
    public final native int isOverrun() /*_const_*/;
    public final native boolean isConsistent() /*_const_*/;
    public final native int/*_NdbDictionary.Event.TableEvent_*/ getEventType() /*_const_*/;
    public final native boolean tableNameChanged() /*_const_*/;
    public final native boolean tableFrmChanged() /*_const_*/;
    public final native boolean tableFragmentationChanged() /*_const_*/;
    public final native boolean tableRangeListChanged() /*_const_*/;
    public final native long/*_Uint64_*/ getGCI() /*_const_*/;
    public final native int/*_Uint32_*/ getAnyValue() /*_const_*/;
    public final native long/*_Uint64_*/ getLatestGCI() /*_const_*/;
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public interface /*_enum_*/ State
    {
        int EO_CREATED = 0 /*__*/,
            EO_EXECUTING = 1 /*__*/,
            EO_DROPPED = 2 /*__*/,
            EO_ERROR = 3 /*__*/;
    }
    public final native int/*_State_*/ getState();
    public final native void mergeEvents(boolean flag);
    public final native int execute();
    public final native NdbRecAttr/*_NdbRecAttr *_*/ getValue(String/*_const char *_*/ anAttrName, ByteBuffer/*_char *_*/ aValue /*_= 0_*/);
    public final native NdbRecAttr/*_NdbRecAttr *_*/ getPreValue(String/*_const char *_*/ anAttrName, ByteBuffer/*_char *_*/ aValue /*_= 0_*/);
    public final native NdbBlob/*_NdbBlob *_*/ getBlobHandle(String/*_const char *_*/ anAttrName);
    public final native NdbBlob/*_NdbBlob *_*/ getPreBlobHandle(String/*_const char *_*/ anAttrName);
}
