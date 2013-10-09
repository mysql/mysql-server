/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.openjpa;

import java.util.BitSet;

import org.apache.openjpa.jdbc.meta.JavaSQLTypes;
import org.apache.openjpa.kernel.OpenJPAStateManager;
import org.apache.openjpa.meta.ClassMetaData;
import org.apache.openjpa.meta.FieldMetaData;

/** Grab bag of utility methods
 *
 */
public class NdbOpenJPAUtility {

    /** JavaType names corresponding to org.apache.openjpa.meta.JavaTypes */
    protected static String[] javaTypeNames = {
        "boolean",          /* 0: boolean */
        "byte",             /* 1: byte */
        "char",             /* 2: char */
        "double",           /* 3: double */
        "float",            /* 4: float */
        "int",              /* 5: int */
        "long",             /* 6: long */
        "short",            /* 7: short */
        "Object",           /* 8: Object */
        "String",           /* 9: String */
        "Number",           /* 10: Number */
        "Array",            /* 11: Array */
        "Collection",       /* 12: Collection */
        "Map",              /* 13: Map */
        "java.util.Date",   /* 14: java.util.Date */
        "PC",               /* 15: PC */
        "Boolean",          /* 16: Boolean */
        "Byte",             /* 17: Byte */
        "Character",        /* 18: Character */
        "Double",           /* 19: Double */
        "Float",            /* 20: Float */
        "Integer",          /* 21: Integer */
        "Long",             /* 22: Long */
        "Short",            /* 23: Short */
        "BigDecimal",       /* 24: BigDecimal */
        "BigInteger",       /* 25: BigInteger */
        "Locale",           /* 26: Locale */
        "PC Untyped",       /* 27: PC Untyped */
        "Calendar",         /* 28: Calendar */
        "OID",              /* 29: OID */
        "InputStream",      /* 30: InputStream */
        "InputReader"       /* 31: InputReader */
    };

    public static String getJavaTypeName(int javaType) {
        if (javaType < javaTypeNames.length) {
            return javaTypeNames[javaType];
        } else {
            switch (javaType) {
                case JavaSQLTypes.SQL_DATE:
                    return "java.sql.Date";
                case JavaSQLTypes.TIME:
                    return "java.sql.Time";
                case JavaSQLTypes.TIMESTAMP:
                    return "java.sql.Timestamp";
                default: return "unsupported";
            }
        }
    }

    public static String printBitSet(OpenJPAStateManager sm, BitSet fields) {
        ClassMetaData classMetaData = sm.getMetaData();
        FieldMetaData[] fieldMetaDatas = classMetaData.getFields();
        StringBuffer buffer = new StringBuffer("[");
        if (fields != null) {
            String separator = "";
            for (int i = 0; i < fields.size(); ++i) {
                if (fields.get(i)) {
                    buffer.append(separator);
                    buffer.append(i);
                    buffer.append(" ");
                    buffer.append(fieldMetaDatas[i].getName());
                    separator = ";";
                }
            }
        }
        buffer.append("] ");
        return buffer.toString();
    }

}
