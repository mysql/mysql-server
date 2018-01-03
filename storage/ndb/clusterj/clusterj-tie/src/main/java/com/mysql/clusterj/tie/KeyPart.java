/*
 *  Copyright 2010 Sun Microsystems, Inc.
 *  Use is subject to license terms.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import java.nio.ByteBuffer;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.ndbjtie.ndbapi.Ndb.Key_part_ptr;

/**
 * This class encapsulates ByteBuffers that hold partition key parts. 
 */
class KeyPart {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(KeyPart.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(KeyPart.class);

    /** The ByteBuffer */
    ByteBuffer buffer;

    /** The length */
    int length;

    public KeyPart(ByteBuffer buffer, int length) {
        this.buffer = buffer;
        this.length = length;
    }

    public void get(Key_part_ptr keyPartPtr) {
        keyPartPtr.ptr(buffer);
        keyPartPtr.len(length);
    }

    public ByteBuffer getBuffer() {
        return buffer;
    }

    public int getLength() {
        return length;
    }

}
