/*
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
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

import java.lang.reflect.Method;
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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

import com.mysql.ndbjtie.mysql.CharsetMap;
import com.mysql.ndbjtie.mysql.CharsetMapConst;
import com.mysql.ndbjtie.mysql.Utils;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbRecAttr;
import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.tie.DbImpl.BufferManager;

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

    /** Standard Java charset */
    static Charset charset = Charset.forName("windows-1252");

    static final long ooooooooooooooff = 0x00000000000000ffL;
    static final long ooooooooooooffoo = 0x000000000000ff00L;
    static final long ooooooooooffoooo = 0x0000000000ff0000L;
    static final long ooooooooffoooooo = 0x00000000ff000000L;
    static final long ooooooffoooooooo = 0x000000ff00000000L;
    static final long ooooffoooooooooo = 0x0000ff0000000000L;
    static final long ooffoooooooooooo = 0x00ff000000000000L;
    static final long ffoooooooooooooo = 0xff00000000000000L;
    static final long ooooooooffffffff = 0x00000000ffffffffL;
    static final int ooooooff = 0x000000ff;
    static final int ooooffff = 0x0000ffff;
    static final int ooooffoo = 0x0000ff00;
    static final int ooffoooo = 0x00ff0000;
    static final int ooffffff = 0x00ffffff;
    static final int ffoooooo = 0xff000000;

    static final char[] SPACE_PAD = new char[255];
    static {
        for (int i = 0; i < 255; ++i) {
            SPACE_PAD[i] = ' ';
        }
    }

    static final byte[] ZERO_PAD = new byte[255];
    static {
        for (int i = 0; i < 255; ++i) {
            ZERO_PAD[i] = (byte)0;
        }
    }
 
    static final byte[] BLANK_PAD = new byte[255];
    static {
        for (int i = 0; i < 255; ++i) {
            BLANK_PAD[i] = (byte)' ';
        }
    }

    static final byte[] EMPTY_BYTE_ARRAY = new byte[0];

    static int MAX_MEDIUMINT_VALUE = (int)(Math.pow(2, 23) - 1);
    static int MAX_MEDIUMUNSIGNED_VALUE = (int)(Math.pow(2, 24) - 1);
    static int MIN_MEDIUMINT_VALUE = (int) (- Math.pow(2, 23));

    /** Scratch buffer pool used for decimal conversions; 65 digits of precision, sign, decimal, null terminator */
    static final FixedByteBufferPoolImpl decimalByteBufferPool = new FixedByteBufferPoolImpl(68, "Decimal Pool");

    /* Error codes that are not severe, and simply reflect expected conditions */
    private static Set<Integer> NonSevereErrorCodes = new HashSet<Integer>();

    public static final int SET_NOT_NULL_TO_NULL = 4203;
    public static final int INDEX_NOT_FOUND = 4243;
    public static final int ROW_NOT_FOUND = 626;
    public static final int DUPLICATE_PRIMARY_KEY = 630;
    public static final int DUPLICATE_UNIQUE_KEY = 893;
    public static final int FOREIGN_KEY_NO_PARENT = 255;
    public static final int FOREIGN_KEY_REFERENCED_ROW_EXISTS = 256;

    static {
        NonSevereErrorCodes.add(SET_NOT_NULL_TO_NULL); // Attempt to set a NOT NULL attribute to NULL
        NonSevereErrorCodes.add(INDEX_NOT_FOUND); // Index not found
        NonSevereErrorCodes.add(ROW_NOT_FOUND); // Tuple did not exist
        NonSevereErrorCodes.add(DUPLICATE_PRIMARY_KEY); // Duplicate primary key on insert
        NonSevereErrorCodes.add(DUPLICATE_UNIQUE_KEY); // Duplicate unique key on insert
        NonSevereErrorCodes.add(FOREIGN_KEY_NO_PARENT); // Foreign key violation; no parent exists
        NonSevereErrorCodes.add(FOREIGN_KEY_REFERENCED_ROW_EXISTS); // Foreign key violation; referenced row exists
    }

    // TODO: this is intended to investigate a class loader issue with Sparc java
    // The idea is to force loading the CharsetMap native class prior to calling the static create method
    // First, make sure that the native library is loaded because CharsetMap depends on it
    static {
        ClusterConnectionServiceImpl.loadSystemLibrary("ndbclient");
    }
    static Class<?> charsetMapClass = loadClass("com.mysql.ndbjtie.mysql.CharsetMap");
    static Class<?> loadClass(String className) {
        try {
            return Class.forName(className);
        } catch (ClassNotFoundException e) {
            throw new ClusterJUserException(local.message("ERR_Loading_Native_Class", className), e);
        }
    }

    // TODO: change this to a weak reference so we can call delete on it when not needed
    /** Note that mysql refers to charset number and charset name, but the number is
    * actually a collation number. The CharsetMap interface thus has methods like
    * getCharsetNumber(String charsetName) but what is returned is actually a collation number.
    */
    static CharsetMap charsetMap = createCharsetMap();

    // TODO: this is intended to investigate a class loader issue with Sparc java
    // The idea is to create the CharsetMap create method in a try/catch block to report the exact error
    static CharsetMap createCharsetMap() {
        StringBuilder builder = new StringBuilder();
        CharsetMap result = null;
        try {
            return CharsetMap.create();
        } catch (Throwable t1) {
            builder.append("CharsetMap.create() threw " + t1.getClass().getName() + ":" + t1.getMessage());
            try {
                Method charsetMapCreateMethod = charsetMapClass.getMethod("create", (Class[])null);
                result = (CharsetMap)charsetMapCreateMethod.invoke(null, (Object[])null);
                builder.append("charsetMapCreateMethod.invoke() succeeded:" + result);
            } catch (Throwable t2) {
                builder.append("charsetMapCreateMethod.invoke() threw " + t2.getClass().getName() + ":" + t2.getMessage());
            }
            throw new ClusterJUserException(builder.toString());
        }
    }

    /** The maximum mysql collation (charset) number. This is hard coded in <mysql>/include/my_sys.h */
    static int MAXIMUM_MYSQL_COLLATION_NUMBER = 256;

    /** The mysql collation number for the standard charset */
    static int collationLatin1 = charsetMap.getCharsetNumber("latin1");

    /** The mysql collation number for UTF16 */
    static protected final int collationUTF16 = charsetMap.getUTF16CharsetNumber();

    /** The mysql charset map */
    public static CharsetMap getCharsetMap() {
        return charsetMap;
    }

    /** The map of charset name to collations that share the charset name */
    private static Map<String, int[]> collationPeersMap = new TreeMap<String, int[]>();

    /** The ClusterJ charset converter for all multibyte charsets */
    private static CharsetConverter charsetConverterMultibyte = new MultiByteCharsetConverter();

    /** Charset converters */
    private static CharsetConverter[] charsetConverters = 
        new CharsetConverter[MAXIMUM_MYSQL_COLLATION_NUMBER + 1];

    /** Initialize the the array of charset converters and the map of known collations that share the same charset */
    static {
        Map<String, List<Integer>> workingCollationPeersMap = new TreeMap<String, List<Integer>>();
        for (int collation = 1; collation <= MAXIMUM_MYSQL_COLLATION_NUMBER; ++collation) {
            String mysqlName = charsetMap.getMysqlName(collation);
            if (mysqlName != null) {
                if ((isMultibyteCollation(collation))) {
                    // multibyte collations all use the multibyte charset converter
                    charsetConverters[collation] = charsetConverterMultibyte;
                } else {
                    // find out if this charset name is already used by another (peer) collation
                    List<Integer> collations = workingCollationPeersMap.get(mysqlName);
                    if (collations == null) {
                        // this is the first collation to use this charset name
                        collations = new ArrayList<Integer>(8);
                        collations.add(collation);
                        workingCollationPeersMap.put(mysqlName, collations);
                    } else {
                        // add this collation to the list of (peer) collations
                        collations.add(collation);
                    }
                }
            }
        }
        
        for (Map.Entry<String, List<Integer>> workingCollationPeers: workingCollationPeersMap.entrySet()) {
            String mysqlName = workingCollationPeers.getKey();
            List<Integer> collations = workingCollationPeers.getValue();
            int[] collationArray = new int[collations.size()];
            int i = 0;
            for (Integer collation: collations) {
                collationArray[i++] = collation;
            }
            collationPeersMap.put(mysqlName, collationArray);
        }
        if (logger.isDetailEnabled()) {
            for (Map.Entry<String, int[]> collationEntry: collationPeersMap.entrySet()) {
                logger.detail("Utility collationMap " + collationEntry.getKey()
                        + " collations : " + Arrays.toString(collationEntry.getValue()));
            }
        }
        // initialize the charset converter for latin1 and its peer collations (after peers are known)
        addCollation(collationLatin1);
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
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "boolean"));
            }
        }

        public boolean getBoolean(Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                    return value == 1;
                case Tinyint:
                    // the value is stored in the top 8 bits
                    return (value >>> 24) == 1;
                default:
                    throw new ClusterJUserException(
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
                    throw new ClusterJUserException(
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
                    throw new ClusterJUserException(
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
                case Mediumunsigned:
                    return ndbRecAttr.u_medium_value();
                case Time:
                case Mediumint:
                    return ndbRecAttr.medium_value();
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public int getInt(Column storeColumn, int value) {
            int result = 0;
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                case Timestamp:
                    return value;
                case Date:
                    // the unsigned value is stored in the top 3 bytes
                    return value >>> 8;
                case Time:
                    // the signed value is stored in the top 3 bytes
                    return value >> 8;
                case Mediumint:
                    // the three high order bytes are the little endian representation
                    // the original is zzyyax00 and the result is aaaxyyzz
                    result |= (value & ffoooooo) >>> 24;
                    result |= (value & ooffoooo) >>> 8;
                    // the ax byte is signed, so shift left 16 and arithmetic shift right 8
                    result |= ((value & ooooffoo) << 16) >> 8;
                    return result;
                case Mediumunsigned:
                    // the three high order bytes are the little endian representation
                    // the original is zzyyxx00 and the result is 00xxyyzz
                    result |= (value & ffoooooo) >>> 24;
                    result |= (value & ooffoooo) >>> 8;
                    result |= (value & ooooffoo) << 8;
                    return result;
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public long getLong(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                    long rawValue = ndbRecAttr.int64_value();
                    return (rawValue >>> 32) | (rawValue << 32);
                case Mediumint:
                    return ndbRecAttr.medium_value();
                case Mediumunsigned:
                    return ndbRecAttr.u_medium_value();
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
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        /** Put the higher order three bytes of the input value into the result as long value.
         * Also preserve the sign of the MSB while shifting
         * @param byteBuffer the byte buffer
         * @param value the input value
         */
        public long get3ByteLong(long value) {
            // the three high order bytes are the little endian representation
            // the original is zzyyxx0000000000 and the result is 0000000000xxyyzz
            long result = 0L;
            result |= (value & ffoooooooooooooo) >>> 56;
            result |= (value & ooffoooooooooooo) >>> 40;
            // the xx byte is signed, so shift left 16 and arithmetic shift right 40
            result |= ((value & ooooffoooooooooo) << 16) >> 40;
            return result;
        }

        public long getLong(Column storeColumn, long value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // the data is stored as two int values
                    return (value >>> 32) | (value << 32);
                case Mediumint:
                    return get3ByteLong(value);
                case Mediumunsigned:
                    // the three high order bytes are the little endian representation
                    // the original is zzyyxx0000000000 and the result is 0000000000xxyyzz
                    long result = 0L;
                    result |= (value & ffoooooooooooooo) >>> 56;
                    result |= (value & ooffoooooooooooo) >>> 40;
                    result |= (value & ooooffoooooooooo) >>> 24;
                    return result;
                case Bigint:
                case Bigunsigned:
                    return value;
                case Datetime:
                    return unpackDatetime(value);
                case Timestamp:
                    return (value >> 32) * 1000L;
                case Date:
                    long packedDate = get3ByteLong(value);
                    return unpackDate((int)packedDate);
                case Time:
                    long packedTime = get3ByteLong(value);
                    return unpackTime((int)packedTime);
                case Datetime2:
                    return unpackDatetime2(storeColumn.getPrecision(), value);
                case Timestamp2:
                    return unpackTimestamp2(storeColumn.getPrecision(), value);
                case Time2:
                    return unpackTime2(storeColumn.getPrecision(), value);
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        /** Put the low order three bytes of the input value into the ByteBuffer as a medium_value.
         * The format for medium value is always little-endian even on big-endian architectures.
         * Do not flip the buffer, as the caller will do that if needed.
         * @param byteBuffer the byte buffer
         * @param value the input value
         */
        public void put3byteInt(ByteBuffer byteBuffer, int value) {
            byteBuffer.put((byte)(value));            
            byteBuffer.put((byte)(value >> 8));
            byteBuffer.put((byte)(value >> 16));
        }

        public void convertValue(ByteBuffer result, Column storeColumn, byte value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored in an int32
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt(value & 0xff);
                    result.flip();
                    return;
                case Tinyint:
                case Year:
                    result.put(value);
                    result.flip();
                    return;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
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

        public void convertValue(ByteBuffer result, Column storeColumn, short value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored in an int32
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt(value & 0xffff);
                    result.flip();
                    return;
                case Smallint:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putShort(value);
                    result.flip();
                    return;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
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
            convertValue(result, storeColumn, value);
            return result;
        }

        public void convertValue(ByteBuffer result, Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt(value);
                    result.flip();
                    return;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                                "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, value);
                    result.flip();
                    return;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                                "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, value);
                    result.flip();
                    return;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public int convertIntValueForStorage(Column storeColumn, int value) {
            int result = 0;
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                    return value;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                                "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    // the high order bytes are the little endian representation
                    // the the original is 00xxyyzz and the result is zzyyxx00
                    result |= (value & ooooooff) << 24;
                    result |= (value & ooooffoo) << 8;
                    result |= (value & ooffoooo) >> 8;
                    return result;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                                "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    // the high order bytes are the little endian representation
                    // the original is 00xxyyzz and the result is zzyyxx00
                    result |= (value & ooooooff) << 24;
                    result |= (value & ooooffoo) << 8;
                    result |= (value & ooffoooo) >> 8;
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public ByteBuffer convertValue(Column storeColumn, long value) {
            ByteBuffer result = ByteBuffer.allocateDirect(8);
            return convertValue(storeColumn, value, result);
        }

        public void convertValue(ByteBuffer result, Column storeColumn, long value) {
            convertValue(storeColumn, value, result);
        }

        public ByteBuffer convertValue(Column storeColumn, long value, ByteBuffer result) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are stored in two int32 fields
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt((int)((value)));
                    result.putInt((int)((value >>> 32)));
                    result.flip();
                    return result;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, (int)value);
                    result.flip();
                    return result;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, (int)value);
                    result.flip();
                    return result;
                case Bigint:
                case Bigunsigned:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(value);
                    result.flip();
                    return result;
                case Date:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packDate(value));
                    result.flip();
                    return result;
                case Datetime:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packDatetime(value));
                    result.flip();
                    return result;
                case Time:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packTime(value));
                    result.flip();
                    return result;
                case Timestamp:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putInt((int)(value/1000L));
                    result.flip();
                    return result;
                case Datetime2:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packDatetime2(storeColumn.getPrecision(), value));
                    result.flip();
                    return result;
                case Time2:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packTime2(storeColumn.getPrecision(), value));
                    result.flip();
                    return result;
                case Timestamp2:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packTimestamp2(storeColumn.getPrecision(), value));
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        /** Put the low order three bytes of the input value into the long as a medium_value.
         * The format for medium value is always little-endian even on big-endian architectures.
         * @param value the input value
         */
        public long put3byteLong(long value) {
            // the high order bytes are the little endian representation
            // the original is 0000000000xxyyzz and the result is zzyyxx0000000000
            long result = 0L;
            result |= (value & ooooooff) << 56;
            result |= (value & ooooffoo) << 40;
            result |= (value & ooffoooo) << 24;
            return result;
        }

        public long convertLongValueForStorage(Column storeColumn, long value) {
            long result = 0L;
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are stored in two int32 fields
                    result |= (value >>> 32);
                    result |= (value << 32);
                    return result;
                case Bigint:
                case Bigunsigned:
                    return value;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    return put3byteLong(value);
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    return put3byteLong(value);
                case Date:
                    long packDate = packDate(value);
                    return put3byteLong(packDate);
                case Datetime:
                    return packDatetime(value);
                case Time:
                    long packTime = packTime(value);
                    return put3byteLong(packTime);
                case Timestamp:
                    // timestamp is an int so put the value into the high bytes
                    // the original is 00000000tttttttt and the result is tttttttt00000000
                    return (value/1000L) << 32;
                case Datetime2:
                    // value is in milliseconds since the epoch
                    return packDatetime2(storeColumn.getPrecision(), value);
                case Time2:
                    // value is in milliseconds since the epoch
                    return packTime2(storeColumn.getPrecision(), value);
                case Timestamp2:
                    // value is in milliseconds since the epoch
                    return packTimestamp2(storeColumn.getPrecision(), value);
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        public int convertByteValueForStorage(Column storeColumn, byte value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored in an int32
                    return value & ooooooff;
                case Tinyint:
                case Year:
                    // other byte values are stored in the high byte of an int
                    return value << 24;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
        }

        public int convertShortValueForStorage(Column storeColumn, short value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored in an int32
                    return value & ooooffff;
                case Smallint:
                    // short values are in the top 16 bits of an int
                    return value << 16;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }

        public long convertLongValueFromStorage(Column storeColumn,
                long fromStorage) {
            // TODO Auto-generated method stub
            return 0;
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
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "boolean"));
            }
        }

        public boolean getBoolean(Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Tinyint:
                    return value == 1;
                default:
                    throw new ClusterJUserException(
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
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
        }

        public short getShort(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bit:
                case Smallint:
                    return ndbRecAttr.short_value();
                default:
                    throw new ClusterJUserException(
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
                case Mediumunsigned:
                    return ndbRecAttr.u_medium_value();
                case Time:
                case Mediumint:
                    return ndbRecAttr.medium_value();
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public int getInt(Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                case Timestamp:
                    return value;
                case Date:
                case Mediumunsigned:
                    return value & ooffffff;
                case Time:
                case Mediumint:
                    // propagate the sign bit from 3 byte medium_int
                    return (value << 8) >> 8;
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public long getLong(Column storeColumn, NdbRecAttr ndbRecAttr) {
            switch (storeColumn.getType()) {
                case Bigint:
                case Bigunsigned:
                case Bit:
                    return ndbRecAttr.int64_value();
                case Mediumint:
                    return ndbRecAttr.medium_value();
                case Mediumunsigned:
                    return ndbRecAttr.u_medium_value();
                case Datetime:
                    return unpackDatetime(ndbRecAttr.int64_value());
                case Timestamp:
                    return ndbRecAttr.int32_value() * 1000L;
                case Date:
                    return unpackDate(ndbRecAttr.int32_value());
                case Time:
                    return unpackTime(ndbRecAttr.int32_value());
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        public long getLong(Column storeColumn, long value) {
            switch (storeColumn.getType()) {
                case Bigint:
                case Bigunsigned:
                case Mediumint:
                case Mediumunsigned:
                case Bit:
                    return value;
                case Datetime:
                    return unpackDatetime(value);
                case Timestamp:
                    return value * 1000L;
                case Date:
                    return unpackDate((int)(value));
                case Time:
                    return unpackTime((int)(value));
                case Datetime2:
                    // datetime2 is stored in big endian format so need to swap the input
                    return unpackDatetime2(storeColumn.getPrecision(), swap(value));
                case Timestamp2:
                    // timestamp2 is stored in big endian format so need to swap the input
                    return unpackTimestamp2(storeColumn.getPrecision(), swap(value));
                case Time2:
                    // time2 is stored in big endian format so need to swap the input
                    return unpackTime2(storeColumn.getPrecision(), swap(value));
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        /** Put the low order three bytes of the input value into the ByteBuffer as a medium_value.
         * The format for medium value is always little-endian even on big-endian architectures.
         * Do not flip the buffer, as the caller will do that if needed.
         * @param byteBuffer the byte buffer
         * @param value the input value
         */
        public void put3byteInt(ByteBuffer byteBuffer, int value) {
            byteBuffer.putInt(value);
            byteBuffer.limit(3);
        }

        public void convertValue(ByteBuffer result, Column storeColumn, byte value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored as int32
                    result.order(ByteOrder.nativeOrder());
                    result.putInt(value & 0xff);
                    result.flip();
                    return;
                case Tinyint:
                case Year:
                    result.order(ByteOrder.nativeOrder());
                    result.put(value);
                    result.flip();
                    return;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
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
            ByteBuffer result = ByteBuffer.allocateDirect(2);
            convertValue(result, storeColumn, value);
            return result;
        }

        public void convertValue(ByteBuffer result, Column storeColumn, short value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Smallint:
                    result.order(ByteOrder.nativeOrder());
                    result.putShort(value);
                    result.flip();
                    return;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }

        public ByteBuffer convertValue(Column storeColumn, int value) {
            ByteBuffer result = ByteBuffer.allocateDirect(4);
            convertValue(result, storeColumn, value);
            return result;
        }

        public void convertValue(ByteBuffer result, Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                    result.order(ByteOrder.nativeOrder());
                    result.putInt(value);
                    result.flip();
                    return;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                                "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, value);
                    result.flip();
                    return;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                                "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, value);
                    result.flip();
                    return;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public int convertIntValueForStorage(Column storeColumn, int value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Int:
                    return value;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    return value;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    return value;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "int"));
            }
        }

        public void convertValue(ByteBuffer result, Column storeColumn, long value) {
            convertValue(storeColumn, value, result);
        }

        public ByteBuffer convertValue(Column storeColumn, long value) {
            ByteBuffer result = ByteBuffer.allocateDirect(8);
            return convertValue(storeColumn, value, result);
        }

        public ByteBuffer convertValue(Column storeColumn, long value, ByteBuffer result) {
            switch (storeColumn.getType()) {
                case Bit:
                case Bigint:
                case Bigunsigned:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    result.putLong(value);
                    result.flip();
                    return result;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, (int)value);
                    result.flip();
                    return result;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, (int)value);
                    result.flip();
                    return result;
                case Datetime:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    result.putLong(packDatetime(value));
                    result.flip();
                    return result;
                case Timestamp:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    result.putInt((int)(value/1000L));
                    result.flip();
                    return result;
                case Date:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packDate(value));
                    result.flip();
                    return result;
                case Time:
                    result.order(ByteOrder.LITTLE_ENDIAN);
                    put3byteInt(result, packTime(value));
                    result.flip();
                    return result;
                case Datetime2:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packDatetime2(storeColumn.getPrecision(), value));
                    result.flip();
                    return result;
                case Time2:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packTime2(storeColumn.getPrecision(), value));
                    result.flip();
                    return result;
                case Timestamp2:
                    result.order(ByteOrder.BIG_ENDIAN);
                    result.putLong(packTimestamp2(storeColumn.getPrecision(), value));
                    result.flip();
                    return result;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        public long convertLongValueForStorage(Column storeColumn, long value) {
            switch (storeColumn.getType()) {
                case Bit:
                case Bigint:
                case Bigunsigned:
                    return value;
                case Mediumint:
                    if (value > MAX_MEDIUMINT_VALUE || value < MIN_MEDIUMINT_VALUE){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    return value;
                case Mediumunsigned:
                    if (value > MAX_MEDIUMUNSIGNED_VALUE || value < 0){
                        throw new ClusterJUserException(local.message(
                            "ERR_Bounds", value, storeColumn.getName(), storeColumn.getType()));
                    }
                    return value;
                case Datetime:
                    return packDatetime(value);
               case Timestamp:
                    return value/1000L;
                case Date:
                    return packDate(value);
                case Time:
                    return packTime(value);
                case Datetime2:
                    // value is in milliseconds since the epoch
                    // datetime2 is in big endian format so need to swap the result 
                    return swap(packDatetime2(storeColumn.getPrecision(), value));
                case Time2:
                    // value is in milliseconds since the epoch
                    // time2 is in big endian format so need to swap the result 
                    return swap(packTime2(storeColumn.getPrecision(), value));
                case Timestamp2:
                    // value is in milliseconds since the epoch
                    // timestamp2 is in big endian format so need to swap the result 
                    return swap(packTimestamp2(storeColumn.getPrecision(), value));
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

        public int convertByteValueForStorage(Column storeColumn, byte value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored as int32
                case Tinyint:
                case Year:
                    return  value & ooooooff;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "byte"));
            }
        }

        public int convertShortValueForStorage(Column storeColumn, short value) {
            switch (storeColumn.getType()) {
                case Bit:
                    // bit fields are always stored as int32
                case Smallint:
                    return value & ooooffff;
                default:
                    throw new ClusterJUserException(local.message(
                            "ERR_Unsupported_Mapping", storeColumn.getType(), "short"));
            }
        }

        public long convertLongValueFromStorage(Column storeColumn, long fromStorage) {
            switch (storeColumn.getType()) {
                case Bigint:
                case Bigunsigned:
                case Bit:
                case Mediumint:
                case Mediumunsigned:
                    return fromStorage;
                case Datetime:
                    return unpackDatetime(fromStorage);
                case Timestamp:
                    return fromStorage * 1000L;
                case Date:
                    return unpackDate((int)fromStorage);
                case Time:
                    return unpackTime((int)fromStorage);
                default:
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Mapping", storeColumn.getType(), "long"));
            }
        }

    };

    protected static interface EndianManager {
        public void put3byteInt(ByteBuffer byteBuffer, int value);
        public int getInt(Column storeColumn, int value);
        public int getInt(Column storeColumn, NdbRecAttr ndbRecAttr);
        public short getShort(Column storeColumn, NdbRecAttr ndbRecAttr);
        public long getLong(Column storeColumn, NdbRecAttr ndbRecAttr);
        public long getLong(Column storeColumn, long value);
        public byte getByte(Column storeColumn, NdbRecAttr ndbRecAttr);
        public ByteBuffer convertValue(Column storeColumn, byte value);
        public ByteBuffer convertValue(Column storeColumn, short value);
        public ByteBuffer convertValue(Column storeColumn, int value);
        public ByteBuffer convertValue(Column storeColumn, long value);
        public void convertValue(ByteBuffer buffer, Column storeColumn, byte value);
        public void convertValue(ByteBuffer buffer, Column storeColumn, short value);
        public void convertValue(ByteBuffer buffer, Column storeColumn, int value);
        public void convertValue(ByteBuffer buffer, Column storeColumn, long value);
        public boolean getBoolean(Column storeColumn, NdbRecAttr ndbRecAttr);
        public boolean getBoolean(Column storeColumn, int value);
        public int convertIntValueForStorage(Column storeColumn, int value);
        public long convertLongValueForStorage(Column storeColumn, long value);
        public long convertLongValueFromStorage(Column storeColumn, long fromStorage);
        public int convertByteValueForStorage(Column storeColumn, byte value);
        public int convertShortValueForStorage(Column storeColumn, short value);
    }

    /** Convert the integer to a value that can be printed
     */
    protected static String hex(int n) {
        return String.format("0x%08X", n);
    }

    /** Convert the long to a value that can be printed
     */
    protected static String hex(long n) {
        return String.format("0x%016X", n);
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
        int requiredLength = storeColumn.getColumnSpace();
        ByteBuffer result = ByteBuffer.allocateDirect(requiredLength);
        convertValue(result, storeColumn, value);
        return result;
    }

    /** Convert the parameter value and store it in a given ByteBuffer that can be passed to ndbjtie.
     * 
     * @param buffer the buffer, positioned at the location to store the value
     * @param storeColumn the column definition
     * @param value the value to be converted
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, byte[] value) {
        int dataLength = value.length;
        int maximumLength = storeColumn.getLength();
        if (dataLength > maximumLength) {
            throw new ClusterJUserException(
                    local.message("ERR_Data_Too_Long",
                    storeColumn.getName(), maximumLength, dataLength));
        }
        int prefixLength = storeColumn.getPrefixLength();
        switch (prefixLength) {
            case 0:
                buffer.put(value);
                if (dataLength < maximumLength) {
                    // pad with 0x00 on right
                    buffer.put(ZERO_PAD, 0, maximumLength - dataLength);
                }
                break;
            case 1:
                buffer.put((byte)dataLength);
                buffer.put(value);
                break;
            case 2:
                buffer.put((byte)(dataLength%256));
                buffer.put((byte)(dataLength/256));
                buffer.put(value);
                break;
            default: 
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Unknown_Prefix_Length",
                            prefixLength, storeColumn.getName()));
        }
        buffer.flip();
    }

    /** Convert a BigDecimal value to the binary decimal form used by MySQL.
     * Store the result in the given ByteBuffer that is already positioned.
     * Use the precision and scale of the column to convert. Values that don't fit
     * into the column throw a ClusterJUserException.
     * @param result the buffer, positioned at the location to store the value
     * @param storeColumn the column metadata
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static void convertValue(ByteBuffer result, Column storeColumn, BigDecimal value) {
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int bytesNeeded = getDecimalColumnSpace(precision, scale);
        // TODO this should be a policy option, perhaps an annotation to fail on truncation
        BigDecimal scaledValue = value.setScale(scale, RoundingMode.HALF_UP);
        // the new value has the same scale as the column
        String stringRepresentation = scaledValue.toPlainString();
        int length = stringRepresentation.length();
        ByteBuffer byteBuffer = decimalByteBufferPool.borrowBuffer();
        CharBuffer charBuffer = CharBuffer.wrap(stringRepresentation);
        // basic encoding
        charset.newEncoder().encode(charBuffer, byteBuffer, true);
        byteBuffer.flip();
        int returnCode = Utils.decimal_str2bin(
                byteBuffer, length, precision, scale, result, bytesNeeded);
        decimalByteBufferPool.returnBuffer(byteBuffer);
        if (returnCode != 0) {
            throw new ClusterJUserException(
                    local.message("ERR_String_To_Binary_Decimal",
                    returnCode, scaledValue, storeColumn.getName(), precision, scale));
        }
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
        charset.newEncoder().encode(charBuffer, byteBuffer, true);
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
     * Store the result in the given ByteBuffer that is already positioned.
     * Use the precision and scale of the column to convert. Values that don't fit
     * into the column throw a ClusterJUserException.
     * @param result the buffer, positioned at the location to store the value
     * @param storeColumn the column metadata
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    public static ByteBuffer convertValue(ByteBuffer result, Column storeColumn, BigInteger value) {
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int bytesNeeded = getDecimalColumnSpace(precision, scale);
        String stringRepresentation = value.toString();
        int length = stringRepresentation.length();
        ByteBuffer byteBuffer = decimalByteBufferPool.borrowBuffer();
        CharBuffer charBuffer = CharBuffer.wrap(stringRepresentation);
        // basic encoding
        charset.newEncoder().encode(charBuffer, byteBuffer, true);
        byteBuffer.flip();
        int returnCode = Utils.decimal_str2bin(
                byteBuffer, length, precision, scale, result, bytesNeeded);
        decimalByteBufferPool.returnBuffer(byteBuffer);
        byteBuffer.flip();
        if (returnCode != 0) {
            throw new ClusterJUserException(
                    local.message("ERR_String_To_Binary_Decimal",
                    returnCode, stringRepresentation, storeColumn.getName(), precision, scale));
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
        charset.newEncoder().encode(charBuffer, byteBuffer, true);
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

    /** Convert the parameter value into a ByteBuffer that can be passed to ndbjtie.
     *
     * @param buffer the ByteBuffer
     * @param storeColumn the column definition
     * @param value the value to be converted
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, double value) {
        buffer.order(ByteOrder.nativeOrder());
        buffer.putDouble(value);
        buffer.flip();
        return;
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

    /** Convert the parameter value into a ByteBuffer that can be passed to ndbjtie.
     *
     * @param buffer the ByteBuffer
     * @param storeColumn the column definition
     * @param value the value to be converted
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, float value) {
        buffer.order(ByteOrder.nativeOrder());
        buffer.putFloat(value);
        buffer.flip();
        return;
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

    /** Convert the parameter value and copy it into a ByteBuffer parameter that can be passed to ndbjtie.
     *
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @param buffer the ByteBuffer
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, byte value) {
        endianManager.convertValue(buffer, storeColumn, value);
    }

    /** Convert the parameter value and copy it into a ByteBuffer parameter that can be passed to ndbjtie.
     *
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @param buffer the ByteBuffer
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, short value) {
        endianManager.convertValue(buffer, storeColumn, value);
    }

    /** Convert the parameter value and copy it into a ByteBuffer parameter that can be passed to ndbjtie.
     *
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @param buffer the ByteBuffer
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, int value) {
        endianManager.convertValue(buffer, storeColumn, value);
    }

    /** Convert the parameter value and copy it into a ByteBuffer parameter that can be passed to ndbjtie.
     *
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @param buffer the ByteBuffer
     */
    public static void convertValue(ByteBuffer buffer, Column storeColumn, long value) {
        endianManager.convertValue(buffer, storeColumn, value);
    }

    /** Encode a String as a ByteBuffer that can be passed to ndbjtie.
     * Put the length information in the beginning of the buffer.
     * Pad fixed length strings with blanks.
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    protected static ByteBuffer convertValue(Column storeColumn, String value) {
        if (value == null) {
            value = "";
        }
        CharSequence chars = value;
        int offset = storeColumn.getPrefixLength();
        if (offset == 0) {
            chars = padString(value, storeColumn);
        }
        ByteBuffer byteBuffer = encodeToByteBuffer(chars, storeColumn.getCharsetNumber(), offset);
        fixBufferPrefixLength(storeColumn.getName(), byteBuffer, offset);
        if (logger.isDetailEnabled()) dumpBytesToLog(byteBuffer, byteBuffer.limit());
        return byteBuffer;
    }

    /** Encode a String as a ByteBuffer that can be passed to ndbjtie in a COND_LIKE filter.
     * There is no length information in the beginning of the buffer.
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    protected static ByteBuffer convertValueForLikeFilter(Column storeColumn, String value) {
        if (value == null) {
            value = "";
        }
        CharSequence chars = value;
        ByteBuffer byteBuffer = encodeToByteBuffer(chars, storeColumn.getCharsetNumber(), 0);
        if (logger.isDetailEnabled()) dumpBytesToLog(byteBuffer, byteBuffer.limit());
        return byteBuffer;
    }

    /** Encode a byte[] as a ByteBuffer that can be passed to ndbjtie in a COND_LIKE filter.
     * There is no length information in the beginning of the buffer.
     * @param storeColumn the column definition
     * @param value the value to be converted
     * @return the ByteBuffer
     */
    protected static ByteBuffer convertValueForLikeFilter(Column storeColumn, byte[] value) {
        if (value == null) {
            value = EMPTY_BYTE_ARRAY;
        }
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(value.length);
        byteBuffer.put(value);
        byteBuffer.flip();
        if (logger.isDetailEnabled()) dumpBytesToLog(byteBuffer, byteBuffer.limit());
        return byteBuffer;
    }

    /** Encode a byte[] into a ByteBuffer that can be passed to ndbjtie in a COND_LIKE filter.
     * There is no length information in the beginning of the buffer.
     * @param buffer the ByteBuffer
     * @param storeColumn the column definition
     * @param value the value to be converted
     */
    protected static void convertValueForLikeFilter(ByteBuffer buffer, Column storeColumn, byte[] value) {
        if (value == null) {
            value = EMPTY_BYTE_ARRAY;
        }
        buffer.put(value);
        buffer.flip();
        return;
    }

    /** Pad the value with blanks on the right.
     * @param value the input value
     * @param storeColumn the store column
     * @return the value padded with blanks on the right
     */
    private static CharSequence padString(CharSequence value, Column storeColumn) {
        CharSequence chars = value;
        int suppliedLength = value.length();
        int requiredLength = storeColumn.getColumnSpace();
        if (suppliedLength > requiredLength) {
            throw new ClusterJUserException(local.message("ERR_Data_Too_Long",
                    storeColumn.getName(), requiredLength, suppliedLength));
        } else if (suppliedLength < requiredLength) {
            // pad to fixed length
            StringBuilder buffer = new StringBuilder(requiredLength);
            buffer.append(value);
            buffer.append(SPACE_PAD, 0, requiredLength - suppliedLength);
            chars = buffer;
        }
        return chars;
    }
    
    /** Pad the value with blanks on the right.
     * @param buffer the input value
     * @param storeColumn the store column
     * @return the buffer padded with blanks on the right
     */
    private static ByteBuffer padString(ByteBuffer buffer, Column storeColumn) {
        int suppliedLength = buffer.limit();
        int requiredLength = storeColumn.getColumnSpace();
        if (suppliedLength > requiredLength) {
            throw new ClusterJUserException(local.message("ERR_Data_Too_Long",
                    storeColumn.getName(), requiredLength, suppliedLength));
        } else if (suppliedLength < requiredLength) {
            //reset buffer's limit
            buffer.limit(requiredLength);
            //pad to fixed length
            buffer.position(suppliedLength);
            buffer.put(BLANK_PAD, 0, requiredLength - suppliedLength);
            buffer.position(0);
        }
        return buffer;
    }

    /** Fix the length information in a buffer based on the length prefix,
     * either 0, 1, or 2 bytes that hold the length information.
     * @param byteBuffer the byte buffer to fix
     * @param offset the size of the length prefix
     */
    public static void fixBufferPrefixLength(String columnName, ByteBuffer byteBuffer, int offset) {
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

    /** Pack milliseconds since the Epoch into a long in database Datetime2 format.
     * Reference: http://dev.mysql.com/doc/internals/en/date-and-time-data-type-representation.html
     * The packed datetime2 integer part is:
     *  1 bit  sign (1= non-negative, 0= negative) [ALWAYS POSITIVE IN MYSQL 5.6]
     * 17 bits year*13+month (year 0-9999, month 1-12)
     *  5 bits day           (0-31)
     *  5 bits hour          (0-23)
     *  6 bits minute        (0-59)
     *  6 bits second        (0-59)
     *  ---------------------------
     * 40 bits = 5 bytes
     * Calendar month is 0 origin so add 1 to get packed month
     * @param millis milliseconds since the Epoch
     * @return the long in big endian packed Datetime2 format
     */
    protected static long packDatetime2(int precision, long millis) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.setTimeInMillis(millis);
        long year = calendar.get(Calendar.YEAR);
        long month = calendar.get(Calendar.MONTH);
        long day = calendar.get(Calendar.DATE);
        long hour = calendar.get(Calendar.HOUR);
        long minute = calendar.get(Calendar.MINUTE);
        long second = calendar.get(Calendar.SECOND);
        long milliseconds = calendar.get(Calendar.MILLISECOND);
        long packedMillis = packFractionalSeconds(precision, milliseconds);
        long result =         0x8000000000000000L  // 1 bit sign
                + (year     * 0x0003400000000000L) // 17 bits year * 13
                + ((month+1)* 0x0000400000000000L) //         + month
                + (day      * 0x0000020000000000L) // 5 bits day 
                + (hour     * 0x0000001000000000L) // 5 bits hour
                + (minute   * 0x0000000040000000L) // 6 bits minute
                + (second   * 0x0000000001000000L) // 6 bits second
                + packedMillis;        // fractional microseconds
        return result;
    }

    protected static long unpackDatetime2(int precision, long packedDatetime2) {
        int yearMonth = (int)((packedDatetime2 & 0x7FFFC00000000000L) >>> 46); // 17 bits year * 13 + month
        int year = yearMonth / 13;
        int month= (yearMonth % 13) - 1; // calendar month is 0-11
        int day =       (int)((packedDatetime2 & 0x00003E0000000000L) >>> 41); // 5 bits day
        int hour =      (int)((packedDatetime2 & 0x000001F000000000L) >>> 36); // 5 bits hour
        int minute =    (int)((packedDatetime2 & 0x0000000FC0000000L) >>> 30); // 6 bits minute
        int second =    (int)((packedDatetime2 & 0x000000003F000000L) >>> 24); // 6 bits second
        int milliseconds = unpackFractionalSeconds(precision, (int)(packedDatetime2 & 0x0000000000FFFFFFL));
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.YEAR, year);
        calendar.set(Calendar.MONTH, month);
        calendar.set(Calendar.DATE, day);
        calendar.set(Calendar.HOUR, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, second);
        calendar.set(Calendar.MILLISECOND, milliseconds);
        return calendar.getTimeInMillis();
    }

    /** Convert milliseconds to packed time2 format
     * Fractional format

     * FSP      nBytes MaxValue MaxValueHex
     * ---      -----  -------- -----------
     * FSP=1    1byte  90               5A
     * FSP=2    1byte  99               63

     * FSP=3    2bytes 9990           2706
     -------------------------------------
     * Current algorithm does not support FSP=4 to FSP=6
     * These will be truncated to FSP=3
     * FSP=4    2bytes 9999           270F

     * FSP=5    3bytes 999990        F4236
     * FSP=6    3bytes 999999        F423F

     * @param precision number of digits of precision, 0 to 6
     * @param milliseconds
     * @return packed fractional seconds in low order 3 bytes
     */
    protected static long packFractionalSeconds(int precision, long milliseconds) {
        switch (precision) {
            case 0:
                if (milliseconds > 0) throwOnTruncation();
                return 0L;
            case 1: // possible truncation
                if (milliseconds % 100 != 0) throwOnTruncation();
                return (milliseconds / 100L)  * 0x00000000000A0000L;
            case 2: // possible truncation
                if (milliseconds % 10 != 0) throwOnTruncation();
                return (milliseconds / 10L)  * 0x0000000000010000L;
            case 3:
            case 4:
                return milliseconds * 0x0000000000000A00L; // milliseconds * 10
            case 5:
            case 6:
                return milliseconds * 1000L; // milliseconds * 1000
            default:
                return 0L;
        }
    }

    /** Unpack fractional seconds to milliseconds
     * @param precision number of digits of precision, 0 to 6
     * @param fraction packed seconds in low order 3 bytes
     * @return number of milliseconds
     */
    protected static int unpackFractionalSeconds(int precision, int fraction) {
        switch (precision) {
            case 0:
                return 0;
            case 1:
                return ((fraction & 0x00FF0000) >>> 16) * 10;
            case 2:
                return ((fraction & 0x00FF0000) >>> 16) * 10;
            case 3:
            case 4:
                return ((fraction & 0x00FFFF00) >>> 8) / 10;
            case 5:
            case 6:
                return (fraction & 0x00FFFFFF) / 1000;
            default:
                return 0;
        }
    }

    /** Throw an exception if truncation is not allowed
     * Not currently implemented.
     */
    protected static void throwOnTruncation() {
    }

    /** Pack milliseconds since the Epoch into a long in database Time2 format.
     *       1 bit sign (1= non-negative, 0= negative)
     *       1 bit unused (reserved for INTERVAL type)
     *      10 bits hour   (0-838)
     *       6 bits minute (0-59) 
     *       6 bits second (0-59) 
     *       --------------------
     *       24 bits = 3 bytes

     * @param millis milliseconds since the Epoch
     * @return the long in big endian packed Time format
     */
    protected static long packTime2(int precision, long millis) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.setTimeInMillis(millis);
        long hour = calendar.get(Calendar.HOUR);
        long minute = calendar.get(Calendar.MINUTE);
        long second = calendar.get(Calendar.SECOND);
        long milliseconds = calendar.get(Calendar.MILLISECOND);
        long packedMillis = packFractionalSeconds(precision, milliseconds);
        long result =     0x8000000000000000L |
                (hour   * 0x0010000000000000L) |
                (minute * 0x0000400000000000L) |
                (second * 0x0000010000000000L) |
                packedMillis << 16;
        return result;
    }

    protected static long unpackTime2(int precision, long value) {
        int hour =     (int)((value & 0x3FF0000000000000L) >>> 52);
        int minute =   (int)((value & 0x000FC00000000000L) >>> 46);
        int second =   (int)((value & 0x00003F0000000000L) >>> 40);
        int fraction = (int)((value & 0x000000FFFFFF0000L) >>> 16);        
           
        int milliseconds = unpackFractionalSeconds(precision, fraction);
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.HOUR, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, second);
        calendar.set(Calendar.MILLISECOND, milliseconds);
        return calendar.getTimeInMillis();
    }

    /** Pack milliseconds since the Epoch into a long in database Timestamp2 format.
     * First four bytes are Unix time format: seconds since the epoch;
     * Fractional part is 0-3 bytes
     * @param millis milliseconds since the Epoch
     * @return the long in big endian packed Timestamp2 format
     */
    protected static long packTimestamp2(int precision, long millis) {
        long milliseconds = millis % 1000; // extract milliseconds
        long seconds = millis/1000; // truncate to seconds
        long packedMillis = packFractionalSeconds(precision, milliseconds);
        long result = (seconds << 32) +
                (packedMillis << 8);
        if (logger.isDetailEnabled()) logger.detail(
                "packTimestamp2 precision: " + precision + " millis: " + millis + " result: " + hex(result));
        return result;
    }

    protected static long unpackTimestamp2(int precision, long value) {
        int fraction = (int)((value & 0x00000000FFFFFF00) >>> 8);
        long result = ((value >>> 32) * 1000) +
                unpackFractionalSeconds(precision, fraction);
        if (logger.isDetailEnabled()) logger.detail(
                "unpackTimestamp2 precision: " + precision + " value: " + hex(value)
                + " fraction: " + hex(fraction) + " result: " + result);
        return result;
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

    private static void dumpBytesToLog(ByteBuffer byteBuffer, int limit) {
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
        ByteBuffer digits = decimalByteBufferPool.borrowBuffer();
        int returnCode = Utils.decimal_bin2str(byteBuffer, length, precision, scale, digits, capacity);
        if (returnCode != 0) {
            decimalByteBufferPool.returnBuffer(digits);
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
            CharBuffer charBuffer;
            charBuffer = charset.decode(digits);
            string = charBuffer.toString();
            return string;
        } finally {
            decimalByteBufferPool.returnBuffer(digits);
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
        int year = (packedDate >>> 4) & 0x7FFF;
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
     * @param collation the collation
     * @return the decoded String
     */
    public static String decode(byte[] array, int collation) {
        if (array == null) return null;
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(array.length);
        byteBuffer.put(array);
        byteBuffer.flip();
        int inputLength = array.length;
        // TODO make this more reasonable
        int outputLength = inputLength * 4;
        ByteBuffer outputByteBuffer = ByteBuffer.allocateDirect(outputLength);
        int[] lengths = new int[] {inputLength, outputLength};
        int returnCode = charsetMap.recode(lengths, collation, collationUTF16, 
                byteBuffer, outputByteBuffer);
        switch (returnCode) {
            case CharsetMapConst.RecodeStatus.RECODE_OK:
                outputByteBuffer.limit(lengths[1]);
                CharBuffer charBuffer = outputByteBuffer.asCharBuffer();
                return charBuffer.toString();
            case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Charset",
                        collation));
            case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Source",
                        collation, lengths[0]));
            case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Buffer_Too_Small",
                        collation, inputLength, outputLength, lengths[0], lengths[1]));
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Return_Code",
                        returnCode));
        }
    }

    /** Decode a ByteBuffer into a String using the charset. The return value
     * is in UTF16 format.
     * 
     * @param inputByteBuffer the byte buffer to be decoded
     * @param collation the collation
     * @return the decoded String
     */
    protected static String decode(ByteBuffer inputByteBuffer, int collation) {
        int inputLength = inputByteBuffer.limit() - inputByteBuffer.position();
        int outputLength = inputLength * 2;
        ByteBuffer outputByteBuffer = ByteBuffer.allocateDirect(outputLength);
        int[] lengths = new int[] {inputLength, outputLength};
        int returnCode = charsetMap.recode(lengths, collation, collationUTF16, 
                inputByteBuffer, outputByteBuffer);
        switch (returnCode) {
            case CharsetMapConst.RecodeStatus.RECODE_OK:
                outputByteBuffer.limit(lengths[1]);
                CharBuffer charBuffer = outputByteBuffer.asCharBuffer();
                return charBuffer.toString();
            case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Charset",
                        collation));
            case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Source",
                        collation, lengths[0]));
            case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Buffer_Too_Small",
                        collation, inputLength, outputLength, lengths[0], lengths[1]));
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Return_Code",
                        returnCode));
        }
    }


    /** Encode a String into a byte[] for storage.
     * This is used by character large objects when mapping text columns.
     * 
     * @param string the String to encode
     * @param collation the collation
     * @return the encoded byte[]
     */
    public static byte[] encode(String string, int collation) {
        ByteBuffer encoded = encodeToByteBuffer(string, collation, 0);
        int length = encoded.limit();
        byte[] result = new byte[length];
        encoded.get(result);
        return result;
    }

    /** Encode a String into a ByteBuffer
     * using the mysql native encoding method.
     * @param string the String to encode
     * @param collation the collation
     * @param prefixLength the length of the length prefix
     * @return the encoded ByteBuffer with position set to prefixLength
     * and limit one past the last converted byte
     */
    private static ByteBuffer encodeToByteBuffer(CharSequence string, int collation, int prefixLength) {
        if (string == null) return null;
        int inputLength = (string.length() * 2);
        ByteBuffer inputByteBuffer = ByteBuffer.allocateDirect(inputLength);
        CharBuffer charBuffer = inputByteBuffer.asCharBuffer();
        charBuffer.append(string);
        int outputLength = (2 * inputLength) + prefixLength;
        ByteBuffer outputByteBuffer = ByteBuffer.allocateDirect(outputLength);
        outputByteBuffer.position(prefixLength);
        int[] lengths = new int[] {inputLength, outputLength - prefixLength};
        int returnCode = charsetMap.recode(lengths, collationUTF16, collation, 
                inputByteBuffer, outputByteBuffer);
        
        switch (returnCode) {
            case CharsetMapConst.RecodeStatus.RECODE_OK:
                outputByteBuffer.limit(prefixLength + lengths[1]);
                return outputByteBuffer;
            case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Charset",
                        collation));
            case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Source",
                        collation, lengths[0]));
            case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Buffer_Too_Small",
                        collation, inputLength, outputLength, lengths[0], lengths[1]));
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Return_Code",
                        returnCode));
        }
    }

    /** Encode a String into a ByteBuffer for storage.
     * 
     * @param input the input String
     * @param storeColumn the store column
     * @param bufferManager the buffer manager with shared buffers
     * @return a byte buffer with prefix length
     */
    public static ByteBuffer encode(String input, Column storeColumn, BufferManager bufferManager) {
        int collation = storeColumn.getCharsetNumber();
        if (logger.isDetailEnabled()) logger.detail("Utility.encode storeColumn: " + storeColumn.getName() +
                " charsetName " + storeColumn.getCharsetName() +
                " charsetNumber " + collation +
                " input '" + input + "'");
        CharsetConverter charsetConverter = getCharsetConverter(collation);
        CharSequence chars = input;
        int prefixLength = storeColumn.getPrefixLength();
        ByteBuffer byteBuffer = charsetConverter.encode(storeColumn.getName(), chars, collation, prefixLength, bufferManager);
        if (prefixLength == 0) {
            padString(byteBuffer, storeColumn);
        }
        return byteBuffer;
    }

    /** Decode a ByteBuffer into a String using the charset. The return value
     * is in UTF16 format.
     * 
     * @param inputByteBuffer the byte buffer to be decoded positioned past the length prefix
     * @param collation the collation
     * @param bufferManager the buffer manager with shared buffers
     * @return the decoded String
     */
    public static String decode(ByteBuffer inputByteBuffer, int collation, BufferManager bufferManager) {
        CharsetConverter charsetConverter = getCharsetConverter(collation);
        return charsetConverter.decode(inputByteBuffer, collation, bufferManager);
    }

    /** Get the charset converter for the given collation. 
     * This is in the inner loop and must be highly optimized for performance.
     * @param collation the collation
     * @return the charset converter for the collation
     */
    private static CharsetConverter getCharsetConverter(int collation) {
        // must be synchronized because the charsetConverters is not synchronized
        // we avoid a race condition where a charset converter is in the process
        // of being created and it's partially visible by another thread in charsetConverters
        synchronized (charsetConverters) {
            if (collation + 1 > charsetConverters.length) {
                // unlikely; only if collations are added beyond existing collation number
                String charsetName = charsetMap.getName(collation);
                logger.warn(local.message("ERR_Charset_Number_Too_Big", collation, charsetName, 
                        MAXIMUM_MYSQL_COLLATION_NUMBER));
                return charsetConverterMultibyte;
            }
            CharsetConverter result = charsetConverters[collation];
            if (result == null) {
                result = addCollation(collation);
            }
            return result;
        }
    }

    /** Create a new charset converter and add it to the collection of charset converters
     * for all collations that share the same charset.
     * 
     * @param collation the collation to add
     * @return the charset converter for the collation
     */
    private static CharsetConverter addCollation(int collation) {
        if (isMultibyteCollation(collation)) {
            return charsetConverters[collation] = charsetConverterMultibyte;
        }
        String charsetName = charsetMap.getMysqlName(collation);
        CharsetConverter charsetConverter = new SingleByteCharsetConverter(collation);
        int[] collations = collationPeersMap.get(charsetName);
        if (collations == null) {
            // unlikely; only if a new collation is added
            collations = new int[] {collation};
            collationPeersMap.put(charsetName, collations);
            logger.warn(local.message("WARN_Unknown_Collation", collation, charsetName));
            return charsetConverter;
        }
        for (int peer: collations) {
            // for each collation that shares the same charset name, set the charset converter
            logger.info("Adding charset converter " + charsetName + " for collation " + peer);
            charsetConverters[peer] = charsetConverter;
        }
        return charsetConverter;
    }

    /** Is the collation multibyte?
     * 
     * @param collation the collation number
     * @return true if the collation uses a multibyte charset; false if the collation uses a single byte charset;
     * and null if the collation is not a valid collation
     */
    private static Boolean isMultibyteCollation(int collation) {
        boolean[] multibyte = charsetMap.isMultibyte(collation);
        return (multibyte == null)?null:multibyte[0];
    }

    /** Utility methods for encoding and decoding Strings.
     */
    protected interface CharsetConverter {

        ByteBuffer encode(String columnName, CharSequence input, int collation, int prefixLength, BufferManager bufferManager);

        String decode(ByteBuffer inputByteBuffer, int collation, BufferManager bufferManager);
    }

    /** Class for encoding and decoding multibyte charsets. A single instance of this class
     * can be shared among all multibyte charsets.
     */
    protected static class MultiByteCharsetConverter implements CharsetConverter {

        /** Encode a String into a ByteBuffer. The input String is copied into a shared byte buffer.
         * The buffer is encoded via the mysql recode method to a shared String storage buffer.
         * If the output buffer is too small, a new buffer is allocated and the encoding is repeated.
         * @param input the input String
         * @param collation the charset number
         * @param prefixLength the prefix length (0, 1, or 2 depending on the type)
         * @param bufferManager the buffer manager with shared buffers
         * @return a byte buffer positioned at zero with the data ready to send to the database
         */
        public ByteBuffer encode(String columnName, CharSequence input, int collation, int prefixLength, BufferManager bufferManager) {
            // input length in bytes is twice String length
            int inputLength = input.length() * 2;
            ByteBuffer inputByteBuffer = bufferManager.copyStringToByteBuffer(input);
            boolean done = false;
            // first try with output length equal input length
            int sizeNeeded = inputLength;
            while (!done) {
                ByteBuffer outputByteBuffer = bufferManager.getStringStorageBuffer(sizeNeeded);
                int outputLength = outputByteBuffer.limit();
                outputByteBuffer.position(prefixLength);
                int[] lengths = new int[] {inputLength, outputLength - prefixLength};
                int returnCode = charsetMap.recode(lengths, collationUTF16, collation, 
                        inputByteBuffer, outputByteBuffer);
                switch (returnCode) {
                    case CharsetMapConst.RecodeStatus.RECODE_OK:
                        outputByteBuffer.limit(prefixLength + lengths[1]);
                        outputByteBuffer.position(0);
                        fixBufferPrefixLength(columnName, outputByteBuffer, prefixLength);
                        return outputByteBuffer;
                    case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                        throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Charset",
                                collation));
                    case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                        throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Source",
                                collation, lengths[0]));
                    case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                        // loop increasing output buffer size until success or run out of memory...
                        sizeNeeded = sizeNeeded * 3 / 2;
                        break;
                    default:
                        throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Return_Code",
                                returnCode));
                }
            }
            return null; // to make compiler happy; we never get here
        }

        /** Decode a byte buffer into a String. The input is decoded by the mysql charset recode method
         * into a shared buffer. Then the shared buffer is used to create the result String.
         * The input byte buffer is positioned just past the length, and its limit is set to one past the
         * characters to decode.
         * @param inputByteBuffer the input byte buffer
         * @param collation the charset number
         * @param bufferManager the buffer manager with shared buffers
         * @return the decoded String
         */
        public String decode(ByteBuffer inputByteBuffer, int collation, BufferManager bufferManager) {
            int inputLength = inputByteBuffer.limit() - inputByteBuffer.position();
            int sizeNeeded = inputLength * 4;
            boolean done = false;
            while (!done) {
                ByteBuffer outputByteBuffer = bufferManager.getStringByteBuffer(sizeNeeded);
                CharBuffer outputCharBuffer = bufferManager.getStringCharBuffer();
                int outputLength = outputByteBuffer.limit();
                outputByteBuffer.position(0);
                outputByteBuffer.limit(outputLength);
                int[] lengths = new int[] {inputLength, outputLength};
                int returnCode = charsetMap.recode(lengths, collation, collationUTF16, 
                        inputByteBuffer, outputByteBuffer);
                switch (returnCode) {
                    case CharsetMapConst.RecodeStatus.RECODE_OK:
                        outputCharBuffer.position(0);
                        // each output character is two bytes for UTF16
                        outputCharBuffer.limit(lengths[1] / 2);
                        return outputCharBuffer.toString();
                    case CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET:
                        throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Charset",
                                collation));
                    case CharsetMapConst.RecodeStatus.RECODE_BAD_SRC:
                        throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Source",
                                collation, lengths[0]));
                    case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                        // try a bigger buffer
                        sizeNeeded = sizeNeeded * 3 / 2;
                        break;
                    default:
                        throw new ClusterJFatalInternalException(local.message("ERR_Decode_Bad_Return_Code",
                                returnCode));
                }
            }
            return null; // never reached; make the compiler happy
        }
    }

    /** Class for encoding and decoding single byte collations.
     */
    protected static class SingleByteCharsetConverter implements CharsetConverter {

        private static final int BYTE_RANGE = (1 + Byte.MAX_VALUE) - Byte.MIN_VALUE;
        private static byte[] allBytes = new byte[BYTE_RANGE];
        // The initial charToByteMap, with all char mappings mapped
        // to (byte) '?', so that unknown characters are mapped to '?'
        // instead of '\0' (which means end-of-string to MySQL).
        private static byte[] unknownCharsMap = new byte[65536];

        static {
            // initialize allBytes with all possible byte values
            for (int i = Byte.MIN_VALUE; i <= Byte.MAX_VALUE; i++) {
                allBytes[i - Byte.MIN_VALUE] = (byte) i;
            }
            // initialize unknownCharsMap to '?' in each position
            for (int i = 0; i < unknownCharsMap.length; i++) {
                unknownCharsMap[i] = (byte) '?'; // use something 'sane' for unknown chars
            }
        }

        /** The byte to char array */
        private char[] byteToChars = new char[BYTE_RANGE];

        /** The char to byte array */
        private byte[] charToBytes = new byte[65536];

        /** Construct a new single byte charset converter. This converter is only used for 
         * charsets that encode to a single byte for any input character.
         * @param collation
         */
        public SingleByteCharsetConverter(int collation) {
            ByteBuffer allBytesByteBuffer = ByteBuffer.allocateDirect(256);
            allBytesByteBuffer.put(allBytes);
            allBytesByteBuffer.flip();
            String allBytesString = Utility.decode(allBytesByteBuffer, collation);
            if (allBytesString.length() != 256) {
                String charsetName = charsetMap.getName(collation);
                throw new ClusterJFatalInternalException(local.message("ERR_Bad_Charset_Decode_All_Chars",
                        collation, charsetName, allBytesString.length()));
            }
            int allBytesLen = allBytesString.length();

            System.arraycopy(unknownCharsMap, 0, this.charToBytes, 0,
                    this.charToBytes.length);

            for (int i = 0; i < BYTE_RANGE && i < allBytesLen; i++) {
                char c = allBytesString.charAt(i);
                this.byteToChars[i] = c;
                this.charToBytes[c] = allBytes[i];
            }
        }

        /** Encode a String into a ByteBuffer. The input String is encoded, character by character,
         * into an output byte[]. Then the output is copied into a shared byte buffer.
         * @param input the input String
         * @param collation the charset number
         * @param prefixLength the prefix length (0, 1, or 2 depending on the type)
         * @param bufferManager the buffer manager with shared buffers
         * @return a byte buffer positioned at zero with the data ready to send to the database
         */
        public ByteBuffer encode(String columnName, CharSequence input, int collation, int prefixLength, BufferManager bufferManager) {
            int length = input.length();
            byte[] outputBytes = new byte[length];
            for (int position = 0; position < length; ++position) {
                outputBytes[position] = charToBytes[input.charAt(position)];
            }
            // input is now encoded; copy to shared output buffer
            ByteBuffer outputByteBuffer = bufferManager.getStringStorageBuffer(length + prefixLength);
            // skip over prefix
            outputByteBuffer.position(prefixLength);
            outputByteBuffer.put(outputBytes);
            outputByteBuffer.flip();
            // adjust the length prefix
            fixBufferPrefixLength(columnName, outputByteBuffer, prefixLength);
            return outputByteBuffer;
        }

        /** Decode a byte buffer into a String. The input byte buffer is copied into a byte[],
         * then encoded byte by byte into an output char[]. Then the result String is created from the char[].
         * The input byte buffer is positioned just past the length, and its limit is set to one past the
         * characters to decode.
         * @param inputByteBuffer the input byte buffer
         * @param collation the charset number
         * @param bufferManager the buffer manager with shared buffers
         * @return the decoded String
         */
        public String decode(ByteBuffer inputByteBuffer, int collation, BufferManager bufferManager) {
            int inputLimit = inputByteBuffer.limit();
            int inputPosition = inputByteBuffer.position();
            int inputSize = inputLimit- inputPosition;
            byte[] inputBytes = new byte[inputSize];
            inputByteBuffer.get(inputBytes);
            char[] outputChars = new char[inputSize];
            for (int position = 0; position < inputSize; ++position) {
                outputChars[position] = byteToChars[inputBytes[position] - Byte.MIN_VALUE];
            }
            // input is now decoded; create a new String from the output
            String result = new String(outputChars);
            return result;
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

    public static boolean getBoolean(Column storeColumn, int value) {
        return endianManager.getBoolean(storeColumn, value);
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

    public static int getInt(Column storeColumn, int value) {
        return endianManager.getInt(storeColumn, value);
    }

    /** Get a long from this ndbRecAttr. 
     * 
     * @param storeColumn the Column
     * @param ndbRecAttr the NdbRecAttr
     * @return the long
     */

    /** Convert a long value from storage.
     * The value stored in the database might be a time, timestamp, date, bit array,
     * or simply a long value. The converted value can be converted into a 
     * time, timestamp, date, bit array, or long value.
     */
    public static long getLong(Column storeColumn, long value) {
        return endianManager.getLong(storeColumn, value);
    }

    /** Convert a long value into a long for storage. The value parameter
     * may be a date (milliseconds since the epoch), a bit array, or simply a long value.
     * The storage format depends on the type of the column and the endian-ness of 
     * the host.
     * @param storeColumn the column
     * @param value the java value
     * @return the storage value
     */
    public static long convertLongValueForStorage(Column storeColumn, long value) {
        return endianManager.convertLongValueForStorage(storeColumn, value);
    }

    /** Convert a byte value into an int for storage. The value parameter
     * may be a bit, a bit array (BIT(1..8) needs to be stored as an int) or a byte value.
     * The storage format depends on the type of the column and the endian-ness of 
     * the host.
     * @param storeColumn the column
     * @param value the java value
     * @return the storage value
     */
    public static int convertByteValueForStorage(Column storeColumn, byte value) {
        return endianManager.convertByteValueForStorage(storeColumn, value);
    }

    /** Convert a short value into an int for storage. The value parameter
     * may be a bit array (BIT(1..16) needs to be stored as an int) or a short value.
     * The storage format depends on the type of the column and the endian-ness of 
     * the host.
     * @param storeColumn the column
     * @param value the java value
     * @return the storage value
     */
    public static int convertShortValueForStorage(Column storeColumn,
            short value) {
        return endianManager.convertShortValueForStorage(storeColumn, value);
    }

    public static int convertIntValueForStorage(Column storeColumn, int value) {
        return endianManager.convertIntValueForStorage(storeColumn, value);
    }

}
