/*
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *  All rights reserved. Use is subject to license terms.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.RoundingMode;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CharsetEncoder;
import java.util.Calendar;
import java.util.HashSet;
import java.util.Set;

import com.mysql.ndbjtie.mysql.CharsetMap;
import com.mysql.ndbjtie.mysql.CharsetMapConst;
import com.mysql.ndbjtie.mysql.Utils;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbRecAttr;
import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** This class provides utility methods.
 *
 */
public class Utility {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(Utility.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(Utility.class);

    static CharsetEncoder charsetEncoder = Charset.forName("windows-1252").newEncoder();

    static CharsetDecoder charsetDecoder = Charset.forName("windows-1252").newDecoder();

    static final long ooooooooooooooff = 0x00000000000000ffL;
    static final long ooooooooooooffoo = 0x000000000000ff00L;
    static final long ooooooooooffoooo = 0x0000000000ff0000L;
    static final long ooooooooffoooooo = 0x00000000ff000000L;
    static final long ooooooffoooooooo = 0x000000ff00000000L;
    static final long ooooffoooooooooo = 0x0000ff0000000000L;
    static final long ooffoooooooooooo = 0x00ff000000000000L;
    static final long ffoooooooooooooo = 0xff00000000000000L;
    static final long ooooooooffffffff = 0x00000000ffffffffL;

    // TODO: change this to a weak reference so we can call delete on it when not needed
    static CharsetMap charsetMap = CharsetMap.create();

    static protected final int charsetUTF16 = charsetMap.getUTF16CharsetNumber();
    
    public static CharsetMap getCharsetMap() {
        return charsetMap;
    }

    /** Determine if the exception is retriable
     * @param ex the exception
     * @return if the status is retriable
     */
    public static boolean isRetriable(ClusterJDatastoreException ex) {
        return NdbErrorConst.Status.TemporaryError == ex.getStatus();
    }

    private final static EndianManager endianManager = ByteOrder.BIG_ENDIAN.equals(ByteOrder.nativeOrder())?
        /*
         * Big Endian algorithms to convert NdbRecAttr buffer into primitive types
         */
        new EndianManager() {

        public boolean getBoolean(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                    return ndbRecAttr.int32_value() == 1;
                case Tinyint:
                    return ndbRecAttr.int8_value() == 1;
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "boolean"));
            }
        }

        public byte getByte(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                    return (byte)ndbRecAttr.int32_value();
                case Tinyint:
                case Year:
                    return ndbRecAttr.int8_value();
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
        }
        public short getShort(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                    return (short)ndbRecAttr.int32_value();
                case Smallint:
                    return ndbRecAttr.short_value();
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }
        public int getInt(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                case Timestamp:
                    return ndbRecAttr.int32_value();
                case Date:
                    return ndbRecAttr.u_medium_value();
                case Time:
                    return ndbRecAttr.medium_value();
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }
        public long getLong(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                    long rawValue = ndbRecAttr.int64_value();
                    return (rawValue >>> 32) | (rawValue << 32);
                case Bigint:
                case Bigunsigned:
                    return ndbRecAttr.int64_value();
                case Datetime:
                    return unpackDatetime(ndbRecAttr.int64_value());
                case Timestamp:
                    return (((long)ndbRecAttr.int32_value()) & ooooooooffffffff) * 1000L;
                case Date:
                    return unpackDate(ndbRecAttr.u_medium_value());
                case Time:
                    return unpackTime(ndbRecAttr.medium_value());
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }
        // the three bytes are packed as little endian
        // low-byte, middle-byte, high-byte
        // do not flip the buffer, as the caller will do that
        public void put3byteInt(ByteBuffer byteBuffer, int value) {
            byteBuffer.put((byte)(value));            
            byteBuffer.put((byte)(value >> 8));
            byteBuffer.put((byte)(value >> 16));
        }

        public int convertShortToInt(Short value) {
            return value << 16;
        }
        public int convertByteToInt(byte value) {
            return value << 24;
        }
        public ByteBuffer convertValue(Column storeColumn, byte value) {
            ByteBuffer result;
            switch (storeColumn.getType()) {
                case Bit:
                    result = ByteBuffer.allocateDirect(4);
                    // bit fields are always stored in an int32
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt(value & 0xff);
                    result.flip();
                    return result;
                case Tinyint:
                case Year:
                    result = ByteBuffer.allocateDirect(1);
                    result.put(value);
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
        }

        public ByteBuffer convertValue(Column storeColumn, short value) {
            ByteBuffer result;
            switch (storeColumn.getType()) {
                case Bit:
                    result = ByteBuffer.allocateDirect(4);
                    // bit fields are always stored in an int32
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt(value & 0xffff);
                    result.flip();
                    return result;
                case Smallint:
                    result = ByteBuffer.allocateDirect(2);
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putShort(value);
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }

        public ByteBuffer convertValue(Column storeColumn, int value) {
            ByteBuffer result = ByteBuffer.allocateDirect(4);
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                    result.order(ByteOrder.BIG_ENDIAN);
                    break;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
            result.putInt(value);
            result.flip();
            return result;
        }

        public ByteBuffer convertValue(Column storeColumn, long value) {
            ByteBuffer result = null;
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are stored in two int32 fields
                    result = ByteBuffer.allocateDirect(8);
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt((int)((value)));
                    result.putInt((int)((value >>> 32)));
                    result.flip();
                    return result;
                case Bigint:
                case Bigunsigned:
                    result = ByteBuffer.allocateDirect(8);
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(value);
                    result.flip();
                    return result;
                case Date:
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packDate(value));
                    result.flip();
                    return result;
                case Datetime:
                    result = ByteBuffer.allocateDirect(8);
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packDatetime(value));
                    result.flip();
                    return result;
                case Time:
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packTime(value));
                    result.flip();
                    return result;
                case Timestamp:
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt((int)(value/1000L));
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

    }:
        /*
         * Little Endian algorithms to convert NdbRecAttr buffer into primitive types
         */
        new EndianManager() {

        public boolean getBoolean(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                    return ndbRecAttr.int32_value() == 1;
                case Tinyint:
                    return ndbRecAttr.int8_value() == 1;
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "boolean"));
            }
        }

        public byte getByte(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                case Tinyint:
                case Year:
                    return ndbRecAttr.int8_value();
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
        }
        public short getShort(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                case Smallint:
                    return ndbRecAttr.short_value();
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }
        public int getInt(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                case Timestamp:
                    return ndbRecAttr.int32_value();
                case Date:
                    return ndbRecAttr.u_medium_value();
                case Time:
                    return ndbRecAttr.medium_value();
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }
        public long getLong(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bigint:
                case Bigunsigned:
                case Bit:
                    return ndbRecAttr.int64_value();
                case Datetime:
                    return unpackDatetime(ndbRecAttr.int64_value());
                case Timestamp:
                    return ndbRecAttr.int32_value() * 1000L;
                case Date:
                    return unpackDate(ndbRecAttr.int32_value());
                case Time:
                    return unpackTime(ndbRecAttr.int32_value());
                default:
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }
        // the first three bytes in the buffer are significant, and the last byte is zero
        // do not flip the buffer, as the caller will do that
        public void put3byteInt(ByteBuffer byteBuffer, int value) {
            byteBuffer.putInt(value);
            byteBuffer.limit(3);
        }


        public int convertShortToInt(Short value) {
            return (int)value;
        }
        public int convertByteToInt(byte value) {
            return (int)value;
        }

        public ByteBuffer convertValue(Column storeColumn, byte value) {
            ByteBuffer result;
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored as int32
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.nativeOrder());
                    result.putInt(value & 0xff);
                    result.flip();
                    return result;
                case Tinyint:
                case Year:
                    result = ByteBuffer.allocateDirect(1);
                    result.order(ByteOrder.nativeOrder());
                    result.put(value);
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }

        public ByteBuffer convertValue(Column storeColumn, short value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Smallint:
                    ByteBuffer result = ByteBuffer.allocateDirect(2);
                    result.order(ByteOrder.nativeOrder());
                    result.putShort(value);
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }

        public ByteBuffer convertValue(Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                    ByteBuffer result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.nativeOrder());
                    result.putInt(value);
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }
        public ByteBuffer convertValue(Column storeColumn, long value) {
            ByteBuffer result = null;
            switch (storeColumn.getType()) {
                case Bit:
                case Bigint:
                case Bigunsigned:
                    result = ByteBuffer.allocateDirect(8);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    result.putLong(value);
                    result.flip();
                    return result;
                case Datetime:
                    result = ByteBuffer.allocateDirect(8);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    result.putLong(packDatetime(value));
                    result.flip();
                    return result;
                case Timestamp:
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    result.putInt((int)(value/1000L));
                    result.flip();
                    return result;
                case Date:
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packDate(value));
                    result.flip();
                    return result;
                case Time:
                    result = ByteBuffer.allocateDirect(4);
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packTime(value));
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

    };

    /* Error codes that are not severe, and simply reflect expected conditions */
    private static Set<Integer> NonSevereErrorCodes = new HashSet<Integer>();
    static {
        NonSevereErrorCodes.add(4203); // Trying to set a NOT NULL attribute to NULL
        NonSevereErrorCodes.add(4243); // Index not found
        NonSevereErrorCodes.add(626); // Tuple did not exist
    }

    protected static interface EndianManager {
        public void put3byteInt(ByteBuffer byteBuffer, int value);
        public int getInt(Column storeColumn, NdbRecAttr ndbRecAttr);
        public short getShort(Column storeColumn, NdbRecAttr ndbRecAttr);
        public long getLong(Column storeColumn, NdbRecAttr ndbRecAttr);
        public byte getByte(Column storeColumn, NdbRecAttr ndbRecAttr);
        public int convertShortToInt(Short value);
        public int convertByteToInt(byte value);
        public ByteBuffer convertValue(Column storeColumn, byte value);
        public ByteBuffer convertValue(Column storeColumn, short value);
        public ByteBuffer convertValue(Column storeColumn, int value);
        public ByteBuffer convertValue(Column storeColumn, long value);
        public boolean getBoolean(Column storeColumn, NdbRecAttr ndbRecAttr);
    }

    /** Swap the bytes in the value, thereby converting a big-endian value
     * into a little-endian value (or vice versa).
     * @param value the value to be swapped
     * @return the swapped value
     */
    protected static short swap(short value) {
        return (short)((0x00ff & (value >>> 8)) | 
                       (0xff00 & (value  << 8)));
    }

    /** Swap the bytes in the value, thereby converting a big-endian value
     * into a little-endian value (or vice versa).
     * @param value the value to be swapped
     * @return the swapped value
     */
    protected static int swap(int value) {
        return   0x000000ff & (value >>> 24) |
                (0x0000ff00 & (value >>> 8)) |
                (0x00ff0000 & (value  << 8)) |
                (0xff000000 & (value  << 24));
    }

    /** Swap the bytes in the value, thereby converting a big-endian value
     * into a little-endian value (or vice versa).
     * @param value the value to be swapped
     * @return the swapped value
     */
    protected static long swap(long value) {
        return   ooooooooooooooff & (value >>> 56) |
                (ooooooooooooffoo & (value >>> 40)) |
                (ooooooooooffoooo & (value >>> 24)) |
                (ooooooooffoooooo & (value >>> 8)) |
                (ooooooffoooooooo & (value  << 8)) |
                (ooooffoooooooooo & (value  << 24)) |
                (ooffoooooooooooo & (value  << 40)) |
                (ffoooooooooooooo & (value  << 56));
    }

    protected static void throwError(Object returnCode, NdbErrorConst ndbError) {
        throwError(returnCode, ndbError, "");
    }

    protected static void throwError(Object returnCode, NdbErrorConst ndbError, String extra) {
        String message = ndbError.message();
        int code = ndbError.code();
        int mysqlCode = ndbError.mysql_code();
        int status = ndbError.status();
        int classification = ndbError.classification();
        String msg = local.message("ERR_NdbJTie", returnCode, code, mysqlCode, 
                status, classification, message, extra);
        if (!NonSevereErrorCodes .contains(code)) {
            logger.error(msg);
        }
        throw new ClusterJDatastoreException(msg, code, mysqlCode, status, classification);
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, byte[] value) {
        int dataLength = value.length;
        int prefixLength = storeColumn.getPrefixLength();
        ByteBuffer result = ByteBuffer.allocateDirect(prefixLength + dataLength);
        result.order(ByteOrder.nativeOrder());
        switch (prefixLength) {
            case 0:
                result.put(value);
                break;
            case 1:
                if (dataLength > 255) {
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Data_Too_Long",
                            storeColumn.getName(), "255", dataLength));
                }
                result.put((byte)dataLength);
                result.put(value);
                break;
            case 2:
                if (dataLength > 8000) {
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Data_Too_Long",
                            storeColumn.getName(), "8000", dataLength));
                }
                result.put((byte)(dataLength%256));
                result.put((byte)(dataLength/256));
                result.put(value);
                break;
            default: 
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Unknown_Prefix_Length",
                            prefixLength, storeColumn.getName()));
        }
        result.flip();
        return result;
    }

    /** Convert a BigDecimal value to the binary decimal form used by MySQL.
     * Use the precision and scale of the column to convert. Values that don't fit
     * into the column throw a ClusterJUserException.
     * @param storeColumn the column metadata
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, BigDecimal value) {
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int bytesNeeded = getDecimalColumnSpace(precision, scale);
        ByteBuffer result = ByteBuffer.allocateDirect(bytesNeeded);
        // TODO this should be a policy option, perhaps an annotation to fail on truncation
        BigDecimal scaledValue = value.setScale(scale, RoundingMode.HALF_UP);
        // the new value has the same scale as the column
        String stringRepresentation = scaledValue.toPlainString();
        int length = stringRepresentation.length();
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(length);
        CharBuffer charBuffer = CharBuffer.wrap(stringRepresentation);
        // basic encoding
        charsetEncoder.encode(charBuffer, byteBuffer, true);
        byteBuffer.flip();
        int returnCode = Utils.decimal_str2bin(
                byteBuffer, length, precision, scale, result, bytesNeeded);
        byteBuffer.flip();
        if (returnCode != 0) {
            throw new ClusterJUserException(
                    local.message("ERR_String_To_Binary_Decimal", 
                    returnCode, scaledValue, storeColumn.getName(), precision, scale));
        }
        return result;
    }

    /** Convert a BigInteger value to the binary decimal form used by MySQL.
     * Use the precision and scale of the column to convert. Values that don't fit
     * into the column throw a ClusterJUserException.
     * @param storeColumn the column metadata
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, BigInteger value) {
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int bytesNeeded = getDecimalColumnSpace(precision, scale);
        ByteBuffer result = ByteBuffer.allocateDirect(bytesNeeded);
        String stringRepresentation = value.toString();
        int length = stringRepresentation.length();
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(length);
        CharBuffer charBuffer = CharBuffer.wrap(stringRepresentation);
        // basic encoding
        charsetEncoder.encode(charBuffer, byteBuffer, true);
        byteBuffer.flip();
        int returnCode = Utils.decimal_str2bin(
                byteBuffer, length, precision, scale, result, bytesNeeded);
        byteBuffer.flip();
        if (returnCode != 0) {
            throw new ClusterJUserException(
                    local.message("ERR_String_To_Binary_Decimal", 
                    returnCode, stringRepresentation, storeColumn.getName(), precision, scale));
        }
        return result;
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, double value) {
        ByteBuffer result = ByteBuffer.allocateDirect(8);
        result.order(ByteOrder.nativeOrder());
        result.putDouble(value);
        result.flip();
        return result;
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, float value) {
        ByteBuffer result = ByteBuffer.allocateDirect(4);
        result.order(ByteOrder.nativeOrder());
        result.putFloat(value);
        result.flip();
        return result;
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, byte value) {
        return endianManager.convertValue(storeColumn, value);
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, short value) {
        return endianManager.convertValue(storeColumn, value);
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, int value) {
        return endianManager.convertValue(storeColumn, value);
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, long value) {
        return endianManager.convertValue(storeColumn, value);
    }

    /** Convert the parameter value to a ByteBuffer that can be passed to ndbjtie.
     * 
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(Column storeColumn, String value) {
        if (value == null) {
            value = "";
        }
        int offset = storeColumn.getPrefixLength();
        ByteBuffer byteBuffer = encodeToByteBuffer(value, storeColumn.getCharsetNumber(), offset);
        int limit = byteBuffer.limit();
        // go back and fill in the length field(s)
        // size of the output char* is current limit minus offset
        int length = limit - offset;
        byteBuffer.position(0);
        switch (offset) {
            case 0:
                break;
            case 1:
                byteBuffer.put((byte)(length % 256));
                break;
            case 2:
                byteBuffer.put((byte)(length % 256));
                byteBuffer.put((byte)(length / 256));
                break;
        }
        // reset the position and limit for return
        byteBuffer.position(0);
        byteBuffer.limit(limit);
        if (logger.isDetailEnabled()) {
            StringBuffer message = new StringBuffer("String position is: ");
            message.append(byteBuffer.position());
            message.append(" limit: ");
            message.append(byteBuffer.limit());
            message.append(" data [");
            while (byteBuffer.hasRemaining()) {
                message.append((int)byteBuffer.get());
                message.append(" ");
            }
            message.append("]");
            logger.detail(message.toString());
            byteBuffer.position(0);
            byteBuffer.limit(limit);
        }
        return byteBuffer;
    }

    /** Pack milliseconds since the Epoch into an int in database Date format.
     * The date is converted into a three-byte value encoded as
     * YYYYx16x32 + MMx32 + DD.
     * Add one to the month since Calendar month is 0-origin.
     * @param millis milliseconds since the Epoch
     * @return the int in packed Date format
     */
    private static int packDate(long millis) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.setTimeInMillis(millis);
        int year = calendar.get(Calendar.YEAR);
        int month = calendar.get(Calendar.MONTH);
        int day = calendar.get(Calendar.DATE);
        int date = (year * 512) + ((month + 1) * 32) + day;
        return date;
    }

    /** Pack milliseconds since the Epoch into an int in database Time format.
     * Subtract one from date to get number of days (date is 1 origin).
     * The time is converted into a three-byte value encoded as
     * DDx240000 + HHx10000 + MMx100 + SS.
     * @param millis milliseconds since the Epoch
     * @return the int in packed Time format
     */
    private static int packTime(long millis) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.setTimeInMillis(millis);
        int year = calendar.get(Calendar.YEAR);
        int month = calendar.get(Calendar.MONTH);
        int day = calendar.get(Calendar.DATE);
        int hour = calendar.get(Calendar.HOUR);
        int minute = calendar.get(Calendar.MINUTE);
        int second = calendar.get(Calendar.SECOND);
        if (month != 0) {
            throw new ClusterJUserException(
                    local.message("ERR_Write_Time_Domain", new java.sql.Time(millis), millis, year, month, day, hour, minute, second));
        }
        int time = ((day - 1) * 240000) + (hour * 10000) + (minute * 100) + second;
        return time;
    }

    /** Pack milliseconds since the Epoch into a long in database Datetime format.
     * The Datetime contains a eight-byte date and time packed as 
     * YYYYx10000000000 + MMx100000000 + DDx1000000 + HHx10000 + MMx100 + SS
     * Calendar month is 0 origin so add 1 to get packed month
     * @param value milliseconds since the Epoch
     * @return the long in packed Datetime format
     */
    protected static long packDatetime(long value) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.setTimeInMillis(value);
        long year = calendar.get(Calendar.YEAR);
        long month = calendar.get(Calendar.MONTH) + 1;
        long day = calendar.get(Calendar.DATE);
        long hour = calendar.get(Calendar.HOUR);
        long minute = calendar.get(Calendar.MINUTE);
        long second = calendar.get(Calendar.SECOND);
        long packedDatetime = (year * 10000000000L) + (month * 100000000L) + (day * 1000000L)
                + (hour * 10000L) + (minute * 100) + second;
        return packedDatetime;
    }

    /** Convert the byte[] into a String to be used for logging and debugging.
     * 
     * @param bytes the byte[] to be dumped
     * @return the String representation
     */
    public static String dumpBytes (byte[] bytes) {
        StringBuffer buffer = new StringBuffer("byte[");
        buffer.append(bytes.length);
        buffer.append("]: [");
        for (int i = 0; i < bytes.length; ++i) {
            buffer.append((int)bytes[i]);
            buffer.append(" ");
        }
        buffer.append("]");
        return buffer.toString();
    }

    /** Convert the byteBuffer into a String to be used for logging and debugging.
     * 
     * @param byteBuffer the byteBuffer to be dumped
     * @return the String representation
     */
    public static String dumpBytes(ByteBuffer byteBuffer) {
        byteBuffer.mark();
        int length = byteBuffer.limit() - byteBuffer.position();
        byte[] dst = new byte[length];
        byteBuffer.get(dst);
        byteBuffer.reset();
        return dumpBytes(dst);
    }

    public static BigDecimal getDecimal(ByteBuffer byteBuffer, int length, int precision, int scale) {
        String decimal = null;
        try {
            decimal = getDecimalString(byteBuffer, length, precision, scale);
            return new BigDecimal(decimal);
        } catch (NumberFormatException nfe) {
            throw new ClusterJUserException(
                    local.message("ERR_Number_Format", decimal, dump(decimal)));
        }
    }
   
    public static BigInteger getBigInteger(ByteBuffer byteBuffer, int length, int precision, int scale) {
        String decimal = null;
        try {
            decimal = getDecimalString(byteBuffer, length, precision, scale);
            return new BigInteger(decimal);
        } catch (NumberFormatException nfe) {
            throw new ClusterJUserException(
                    local.message("ERR_Number_Format", decimal, dump(decimal)));
        }
    }
   
    /** Get a Decimal String from the byte buffer.
     * 
     * @param byteBuffer the byte buffer with the raw data, starting at position()
     * @param length the length of the data
     * @param precision the precision of the data
     * @param scale the scale of the data
     * @return the Decimal String representation of the value
     */
    public static String getDecimalString(ByteBuffer byteBuffer, int length, int precision, int scale) {
        // allow for decimal point and sign and one more for trailing null
        int capacity = precision + 3;
        ByteBuffer digits = ByteBuffer.allocateDirect(capacity);
        int returnCode = Utils.decimal_bin2str(byteBuffer, length, precision, scale, digits, capacity);
        if (returnCode != 0) {
            throw new ClusterJUserException(
                    local.message("ERR_Binary_Decimal_To_String", 
                    returnCode, precision, scale, dumpBytes(byteBuffer)));
        }
        String string = null;
        // look for the end (null) of the result string
        for (int i = 0; i < digits.limit(); ++i) {
            if (digits.get(i) == 0) {
                // found the end; mark it so we only decode the answer characters
                digits.limit(i);
                break;
            }
        }
        try {
            // use basic decoding
            CharBuffer charBuffer = charsetDecoder.decode(digits);
            string = charBuffer.toString();
            return string;
        } catch (CharacterCodingException e) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Character_Encoding", string));
        }
        
    }

    /** Unpack a Date from its packed int representation.
     * Date is a three-byte integer packed as YYYYx16x32 + MMx32 + DD
     * @param packedDate the packed representation
     * @return the long value as milliseconds since the Epoch
     */
    public static long unpackDate(int packedDate) {
        int date = packedDate & 0x1f;
        packedDate = packedDate >>> 5;
        int month = (packedDate & 0x0f) - 1; // Month value is 0-based. e.g., 0 for January.
        int year = packedDate >>> 4;
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(year, month, date);
        return calendar.getTimeInMillis();
    }

    /** Unpack a Time from its packed int representation.
     * Time is a three-byte integer packed as DDx240000 + HHx10000 + MMx100 + SS
     * @param packedTime the packed representation
     * @return the long value as milliseconds since the Epoch
     */
    public static long unpackTime(int packedTime) {
        int second = packedTime % 100;
        packedTime /= 100;
        int minute = packedTime % 100;
        packedTime /= 100;
        int hour = packedTime % 24;
        int date = (packedTime / 24) + 1;
        if (date > 31) {
            throw new ClusterJUserException(
                    local.message("ERR_Read_Time_Domain", packedTime, date, hour, minute, second));
        }
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.DATE, date);
        calendar.set(Calendar.HOUR, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, second);
        calendar.set(Calendar.MILLISECOND, 0);
        return calendar.getTimeInMillis();
    }

    /** Unpack a Datetime from its packed long representation.
     * The Datetime contains a long packed as 
     * YYYYx10000000000 + MMx100000000 + DDx1000000 + HHx10000 + MMx100 + SS
     * Calendar month is 0 origin so subtract 1 from packed month
     * @param packedDatetime the packed representation
     * @return the value as milliseconds since the Epoch
     */
    protected static long unpackDatetime(long packedDatetime) {
        int second = (int)(packedDatetime % 100);
        packedDatetime /= 100;
        int minute = (int)(packedDatetime % 100);
        packedDatetime /= 100;
        int hour = (int)(packedDatetime % 100);
        packedDatetime /= 100;
        int day = (int)(packedDatetime % 100);
        packedDatetime /= 100;
        int month = (int)(packedDatetime % 100) - 1;
        int year = (int)(packedDatetime / 100);
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.YEAR, year);
        calendar.set(Calendar.MONTH, month);
        calendar.set(Calendar.DATE, day);
        calendar.set(Calendar.HOUR, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, second);
        calendar.set(Calendar.MILLISECOND, 0);
        return calendar.getTimeInMillis();
        
    }

    /** Decode a byte[] into a String using the charset. The return value
     * is in UTF16 format.
     * 
     * @param array the byte[] to be decoded
     * @param charsetNumber the charset number
     * @return the decoded String
     */
    public static String decode(byte[] array, int charsetNumber) {
        if (array == null) return null;
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(array.length);
        byteBuffer.put(array);
        byteBuffer.flip();
        int inputLength = array.length;
        // TODO make this more reasonable
        int outputLength = inputLength * 4;
        ByteBuffer outputByteBuffer = ByteBuffer.allocateDirect(outputLength);
        int[] lengths = new int[] {inputLength, outputLength};
        int returnCode = charsetMap.recode(lengths, charsetNumber, charsetUTF16, 
                byteBuffer, outputByteBuffer);
        switch (returnCode) {
            case CharsetMapConst.RecodeStatus.RECODE_OK:
                outputByteBuffer.limit(lengths[1]);
                CharBuffer charBuffer = outputByteBuffer.asCharBuffer();
                return charBuffer.toString();
            case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Charset",
                        charsetNumber));
            case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Source",
                        charsetNumber, lengths[0]));
            case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Buffer_Too_Small",
                        charsetNumber, inputLength, outputLength, lengths[0], lengths[1]));
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Return_Code",
                        returnCode));
        }
    }

    /** Encode a String into a byte[] for storage
     * 
     * @param string the String to encode
     * @param charsetNumber the charset number
     * @return the encoded byte[]
     */
    public static byte[] encode(String string, int charsetNumber) {
        ByteBuffer encoded = encodeToByteBuffer(string, charsetNumber, 0);
        int length = encoded.limit();
        byte[] result = new byte[length];
        encoded.get(result);
        return result;
    }

    /** Encode a String into a ByteBuffer
     * 
     * @param string the String to encode
     * @param charsetNumber the charset number
     * @param prefixLength the length of the length prefix
     * @return the encoded ByteBuffer with position set to prefixLength
     * and limit one past the last converted byte
     */
    public static ByteBuffer encodeToByteBuffer(String string, int charsetNumber, int prefixLength) {
        if (string == null) return null;
        int inputLength = (string.length() * 2);
        ByteBuffer inputByteBuffer = ByteBuffer.allocateDirect(inputLength);
        CharBuffer charBuffer = inputByteBuffer.asCharBuffer();
        charBuffer.append(string);
        // TODO make this more reasonable
        int outputLength = (2 * inputLength) + prefixLength;
        ByteBuffer outputByteBuffer = ByteBuffer.allocateDirect(outputLength);
        outputByteBuffer.position(prefixLength);
        int[] lengths = new int[] {inputLength, outputLength - prefixLength};
        int returnCode = charsetMap.recode(lengths, charsetUTF16, charsetNumber, 
                inputByteBuffer, outputByteBuffer);
        
        switch (returnCode) {
            case CharsetMapConst.RecodeStatus.RECODE_OK:
                outputByteBuffer.limit(prefixLength + lengths[1]);
                return outputByteBuffer;
            case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Charset",
                        charsetNumber));
            case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Source",
                        charsetNumber, lengths[0]));
            case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Buffer_Too_Small",
                        charsetNumber, inputLength, outputLength, lengths[0], lengths[1]));
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Return_Code",
                        returnCode));
        }
    }

    private static String dump(String string) {
        StringBuffer buffer = new StringBuffer("[");
        for (int i = 0; i < string.length(); ++i) {
            int theCharacter = string.charAt(i);
            buffer.append(theCharacter);
            buffer.append(" ");
        }
        buffer.append("]");
        return buffer.toString();
    }

    /** For each group of 9 decimal digits, the number of bytes needed
     * to represent that group of digits:
     * 10, 100 -> 1; 256
     * 1,000, 10,000 -> 2; 65536
     * 100,000, 1,000,000 -> 3 16,777,216
     * 10,000,000, 100,000,000, 1,000,000,000 -> 4
     */
    static int[] howManyBytesNeeded = new int[] {0,  1,  1,  2,  2,  3,  3,  4,  4,  4,
                                                     5,  5,  6,  6,  7,  7,  8,  8,  8,
                                                     9,  9, 10, 10, 11, 11, 12, 12, 12,
                                                    13, 13, 14, 14, 15, 15, 16, 16, 16,
                                                    17, 17, 18, 18, 19, 19, 20, 20, 20,
                                                    21, 21, 22, 22, 23, 23, 24, 24, 24,
                                                    25, 25, 26, 26, 27, 27, 28, 28, 28,
                                                    29, 29};
    /** Get the number of bytes needed in memory to represent the decimal number.
     * 
     * @param precision the precision of the number
     * @param scale the scale
     * @return the number of bytes needed for the binary representation of the number
     */
    public static int getDecimalColumnSpace(int precision, int scale) {
        int howManyBytesNeededForIntegral = howManyBytesNeeded[precision - scale];
        int howManyBytesNeededForFraction = howManyBytesNeeded[scale];
        int result = howManyBytesNeededForIntegral + howManyBytesNeededForFraction;
        return result;
    }

    /** Get a boolean from this ndbRecAttr. 
     * 
     * @param storeColumn the Column
     * @param ndbRecAttr the NdbRecAttr
     * @return the boolean
     */
    public static boolean getBoolean(Column storeColumn, NdbRecAttr ndbRecAttr) {
        return endianManager.getBoolean(storeColumn, ndbRecAttr);
    }

    /** Get a byte from this ndbRecAttr. 
     * 
     * @param storeColumn the Column
     * @param ndbRecAttr the NdbRecAttr
     * @return the byte
     */
    public static byte getByte(Column storeColumn, NdbRecAttr ndbRecAttr) {
        return endianManager.getByte(storeColumn, ndbRecAttr);
    }

    /** Get a short from this ndbRecAttr. 
     * 
     * @param storeColumn the Column
     * @param ndbRecAttr the NdbRecAttr
     * @return the short
     */
    public static short getShort(Column storeColumn, NdbRecAttr ndbRecAttr) {
        return endianManager.getShort(storeColumn, ndbRecAttr);
    }

    /** Get an int from this ndbRecAttr. 
     * 
     * @param storeColumn the Column
     * @param ndbRecAttr the NdbRecAttr
     * @return the int
     */
    public static int getInt(Column storeColumn, NdbRecAttr ndbRecAttr) {
        return endianManager.getInt(storeColumn, ndbRecAttr);
    }

    /** Get a long from this ndbRecAttr. 
     * 
     * @param storeColumn the Column
     * @param ndbRecAttr the NdbRecAttr
     * @return the long
     */
    public static long getLong(Column storeColumn, NdbRecAttr ndbRecAttr) {
        return endianManager.getLong(storeColumn, ndbRecAttr);
    }

    /**
     * Convert a short value into an int for the purpose of storing into a value buffer.
     * @param value a short
     * @return the int which when stored will have the short in the right place
     */
    public static int convertShortToInt(Short value) {
        return endianManager.convertShortToInt(value);
    }

    /**
     * Convert a byte value into an int for the purpose of storing into a value buffer.
     * @param value a byte
     * @return the int which when stored will have the byte in the right place
     */
    public static int convertByteToInt(byte value) {
        return endianManager.convertByteToInt(value);
    }

}
