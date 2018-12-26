/*
   Copyright 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJUserException;
import testsuite.clusterj.model.NullValues;

public class NullValuesTest extends AbstractClusterJModelTest {

    protected static final String tablename = "nullvalues";
    protected static final int numberOfPropertyTypes = 7;
    protected static final int numberOfFieldsPerProperty = 12;

    /** The sum of the following values is the switch index into the
     * field setting method.
     */
    protected static final int DATABASE_NOT_NULL = 0; //  if the column is defined as not nullable
    protected static final int DATABASE_NULL =  1; // if the column is defined as nullable
    protected static final String[] databaseNull =
            new String[] {"DATABASE_NOT_NULL", "DATABASE_NULL"};
    protected static final int DATABASE_NO_DEFAULT = 0; // if the column has no default value in the database schema
    protected static final int DATABASE_DEFAULT = 2; // if the column has a default value in the database schema
    protected static final String[] databaseDefault =
            new String[] {"DATABASE_NO_DEFAULT", "DATABASE_DEFAULT"};
    protected static final int NULLVALUE_NONE = 0; // if the property is NullValue.NONE
    protected static final int NULLVALUE_EXCEPTION = 4; // if the property is NullValue.EXCEPTION
    protected static final int NULLVALUE_DEFAULT = 8; // if the property is NullValue.DEFAULT
    protected static final String[] nullValue =
            new String[] {"NULL_VALUE_NONE", "NULL_VALUE_EXCEPTION", "NULL_VALUE_DEFAULT"};

    protected static final String[] propertyTypes = new String[] {
        "Integer", "Long", "Short", "Byte", "String", "Float", "Double"};

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.deletePersistentAll(NullValues.class);
        addTearDownClasses(NullValues.class);
    }

    /** Test that the behavior of the NullValue annotation is effective.
     * For each column type, test the NullValue annotation behavior for
     * null values:
     * NullValue.NONE: nothing is done by ClusterJ, so the behavior of the
     * database takes over, which might result in an exception
     * or a default value being written
     * NullValue.EXCEPTION: ClusterJ throws an exception
     * NullValue.DEFAULT: ClusterJ sets the value to the default value as
     * defined in ClusterJ metadata
     * The test makes use of a class, NullValues, that contains properties
     * of all nullable types and all NullValue annotations on those types.
     * Each property is assigned a number for the test.
     *
     */
    public void test() {

        tx = session.currentTransaction();
        // make sure we can commit a legal instance.
        if (true) {
            tx.begin();
            NullValues instance = createValidInstance();
            int id = 0;
            instance.setId(id);
            session.makePersistent(instance);
            tx.commit();
            tx.begin();
            instance = session.find(NullValues.class, id);
            session.deletePersistent(instance);
            tx.commit();
        }
        // iterate all property and field types and set the value of the
        // field to null and then commit.
        for (int propertyType = 0; propertyType < numberOfPropertyTypes; ++propertyType) {
            for (int fieldIndex = 0; fieldIndex < numberOfFieldsPerProperty; ++fieldIndex) {
                NullValues instance = createValidInstance();
                int id = propertyType * numberOfFieldsPerProperty + fieldIndex;
                //System.out.println("id: " + id);
                instance.setId(id);
                try {
                    tx.begin();
                    // set one property to null value
                    setValue(instance, propertyType, fieldIndex, true);
                    // and see what happens
                    session.makePersistent(instance);
                    tx.commit();
                    //System.out.println("no exception for: " + id + " " + decodeValues(id));
                    if (expectedException(id, null)) {
                        error("Expected exception not thrown for case " +
                                decodeType(propertyType) + ": " + decodeValues(id));
                    }
                } catch (Exception e) {
                    // see if this is an expected exception
                    //System.out.println("exception for: " + id + " " + decodeValues(id));
                    if (!expectedException(id, e)) {
                        error("Unexpected exception from commit for case " +
                                decodeType(propertyType) + ": "  + decodeValues(id)
                                + " " + e.toString());
                        e.printStackTrace();
                    }
                    continue; // if an exception was caught, don't try to verify the instance
                } finally {
                    if (tx.isActive()) {
                        tx.rollback();
                    }
                }
                try {
                    tx.begin();
                    instance = session.find(NullValues.class, id);
                    verifyNullValues(propertyType, fieldIndex, id, instance);
                    tx.commit();
                } catch (Exception e) {
                    error("Unexpected exception from find for case" +
                            decodeType(propertyType) + ": "  + decodeValues(id)
                            + " " + e.toString());
                        e.printStackTrace();
                }
            }
        }
        failOnError();
    }

    protected NullValues createValidInstance() {
        NullValues instance = session.newInstance(NullValues.class);
        for (int propertyType = 0; propertyType < numberOfPropertyTypes; ++propertyType) {
            for (int fieldIndex = 0; fieldIndex < numberOfFieldsPerProperty; ++fieldIndex) {
                // set all properties to non-null values
                setValue(instance, propertyType, fieldIndex, false);
            }
        }
        return instance;
    }

    protected int whatDatabaseDefault(int id) {
        return ((id % numberOfFieldsPerProperty) & 2) >> 1;
    }

    protected int whatDatabaseNull(int id) {
        return (id % numberOfFieldsPerProperty) & 1;
    }

    protected int whatNullValue(int id) {
        return ((id % numberOfFieldsPerProperty) & 12) >> 2;
    }

    private String decodeValues(int id) {
//    protected static final int DATABASE_NOT_NULL = 0; //  if the column is defined as not nullable
//    protected static final int DATABASE_NULL =  1; // if the column is defined as nullable
        String databaseNullResult = databaseNull[whatDatabaseNull(id)];
//    protected static final int DATABASE_NO_DEFAULT = 0; // if the column has no default value in the database schema
//    protected static final int DATABASE_DEFAULT = 2; // if the column has a default value in the database schema
        String databaseDefaultResult = databaseDefault[whatDatabaseDefault(id)];
//    protected static final int NULLVALUE_NONE = 0; // if the property is NullValue.NONE
//    protected static final int NULLVALUE_EXCEPTION = 4; // if the property is NullValue.EXCEPTION
//    protected static final int NULLVALUE_DEFAULT = 8; // if the property is NullValue.DEFAULT
        String nullValueResult = nullValue[whatNullValue(id)];
        return databaseNullResult + " " +
                databaseDefaultResult + " " +
                nullValueResult;
    }

    private String decodeType(int type) {
        return propertyTypes[type];
    }

    private boolean expectedException(int id, Exception e) {
        // if DATABASE_NOT_NULL and NULL_VALUE_NONE => ClusterJDatastoreException
        // the database exception might have been wrapped
        Throwable t = e==null?null:e.getCause();
        if (whatDatabaseNull(id) == 0 &&
                (whatNullValue(id) == 0)) {
            if (e != null &&
                    e instanceof ClusterJDatastoreException) {
                return true;
            } else if (t instanceof Exception) {
                    return expectedException(id, (Exception)t);
            } else {
                return false;
            }
        }
        // if NULL_VALUE_EXCEPTION => ClusterJUserException
        if (whatNullValue(id) == 1) {
            if (e != null &&
                    e instanceof ClusterJUserException) {
                return true;
            } else {
                return false;
            }
        }
        // else no exception is expected
        return false;
    }

    private void verifyNullValues(int propertyType, int fieldIndex, int id, NullValues instance) {
        Object value = getValue(instance, propertyType, fieldIndex);
        if (whatDatabaseNull(id) == DATABASE_NULL
                && (whatDatabaseDefault(id) == DATABASE_NO_DEFAULT)
                && (whatNullValue(id) == NULLVALUE_NONE)
                ) {
            // should store a null in the database for this case
            errorIfNotEqual("For id " + id + " propertyType " + propertyType + " fieldIndex " + fieldIndex + " instance was " + (instance==null?"null":instance.toString() + " value was " + ((value==null)?"null ":value + " of type " + value.getClass().getName() + " " ) + decodeValues(id)), null, value);
        }
    }

    /** Set the value of a specific field to either a null or not-null value.
     *
     * @param instance the instance to set values for
     * @param propertyType the property type
     * @param fieldIndex the field within the property
     * @param nullValue set to a null value
     */
    private void setValue(NullValues instance, int propertyType, int fieldIndex, boolean nullValue) {
        switch(propertyType) {
            case 0: // Integer
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setIntNotNullNoDefaultNullValueNone(nullValue?null:0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setIntNullNoDefaultNullValueNone(nullValue?null:0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setIntNotNullDefaultNullValueNone(nullValue?null:0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setIntNullDefaultNullValueNone(nullValue?null:0); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setIntNotNullNoDefaultNullValueException(nullValue?null:0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setIntNullNoDefaultNullValueException(nullValue?null:0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setIntNotNullDefaultNullValueException(nullValue?null:0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setIntNullDefaultNullValueException(nullValue?null:0); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setIntNotNullNoDefaultNullValueDefault(nullValue?null:0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setIntNullNoDefaultNullValueDefault(nullValue?null:0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setIntNotNullDefaultNullValueDefault(nullValue?null:0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setIntNullDefaultNullValueDefault(nullValue?null:0); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
                break;
            case 1: // Long
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setLongNotNullNoDefaultNullValueNone(nullValue?null:0L); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setLongNullNoDefaultNullValueNone(nullValue?null:0L); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setLongNotNullDefaultNullValueNone(nullValue?null:0L); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setLongNullDefaultNullValueNone(nullValue?null:0L); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setLongNotNullNoDefaultNullValueException(nullValue?null:0L); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setLongNullNoDefaultNullValueException(nullValue?null:0L); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setLongNotNullDefaultNullValueException(nullValue?null:0L); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setLongNullDefaultNullValueException(nullValue?null:0L); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setLongNotNullNoDefaultNullValueDefault(nullValue?null:0L); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setLongNullNoDefaultNullValueDefault(nullValue?null:0L); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setLongNotNullDefaultNullValueDefault(nullValue?null:0L); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setLongNullDefaultNullValueDefault(nullValue?null:0L); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
            case 2: // Short
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setShortNotNullNoDefaultNullValueNone(nullValue?null:(short)0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setShortNullNoDefaultNullValueNone(nullValue?null:(short)0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setShortNotNullDefaultNullValueNone(nullValue?null:(short)0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setShortNullDefaultNullValueNone(nullValue?null:(short)0); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setShortNotNullNoDefaultNullValueException(nullValue?null:(short)0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setShortNullNoDefaultNullValueException(nullValue?null:(short)0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setShortNotNullDefaultNullValueException(nullValue?null:(short)0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setShortNullDefaultNullValueException(nullValue?null:(short)0); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setShortNotNullNoDefaultNullValueDefault(nullValue?null:(short)0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setShortNullNoDefaultNullValueDefault(nullValue?null:(short)0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setShortNotNullDefaultNullValueDefault(nullValue?null:(short)0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setShortNullDefaultNullValueDefault(nullValue?null:(short)0); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
            case 3: // Byte
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setByteNotNullNoDefaultNullValueNone(nullValue?null:(byte)0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setByteNullNoDefaultNullValueNone(nullValue?null:(byte)0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setByteNotNullDefaultNullValueNone(nullValue?null:(byte)0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setByteNullDefaultNullValueNone(nullValue?null:(byte)0); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setByteNotNullNoDefaultNullValueException(nullValue?null:(byte)0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setByteNullNoDefaultNullValueException(nullValue?null:(byte)0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setByteNotNullDefaultNullValueException(nullValue?null:(byte)0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setByteNullDefaultNullValueException(nullValue?null:(byte)0); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setByteNotNullNoDefaultNullValueDefault(nullValue?null:(byte)0); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setByteNullNoDefaultNullValueDefault(nullValue?null:(byte)0); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setByteNotNullDefaultNullValueDefault(nullValue?null:(byte)0); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setByteNullDefaultNullValueDefault(nullValue?null:(byte)0); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
            case 4: // String
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setStringNotNullNoDefaultNullValueNone(nullValue?null:"0"); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setStringNullNoDefaultNullValueNone(nullValue?null:"0"); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setStringNotNullDefaultNullValueNone(nullValue?null:"0"); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setStringNullDefaultNullValueNone(nullValue?null:"0"); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setStringNotNullNoDefaultNullValueException(nullValue?null:"0"); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setStringNullNoDefaultNullValueException(nullValue?null:"0"); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setStringNotNullDefaultNullValueException(nullValue?null:"0"); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setStringNullDefaultNullValueException(nullValue?null:"0"); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setStringNotNullNoDefaultNullValueDefault(nullValue?null:"0"); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setStringNullNoDefaultNullValueDefault(nullValue?null:"0"); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setStringNotNullDefaultNullValueDefault(nullValue?null:"0"); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setStringNullDefaultNullValueDefault(nullValue?null:"0"); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
                break;
            case 5: // Float
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setFloatNotNullNoDefaultNullValueNone(nullValue?null:0.0F); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setFloatNullNoDefaultNullValueNone(nullValue?null:0.0F); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setFloatNotNullDefaultNullValueNone(nullValue?null:0.0F); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setFloatNullDefaultNullValueNone(nullValue?null:0.0F); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setFloatNotNullNoDefaultNullValueException(nullValue?null:0.0F); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setFloatNullNoDefaultNullValueException(nullValue?null:0.0F); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setFloatNotNullDefaultNullValueException(nullValue?null:0.0F); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setFloatNullDefaultNullValueException(nullValue?null:0.0F); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setFloatNotNullNoDefaultNullValueDefault(nullValue?null:0.0F); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setFloatNullNoDefaultNullValueDefault(nullValue?null:0.0F); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setFloatNotNullDefaultNullValueDefault(nullValue?null:0.0F); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setFloatNullDefaultNullValueDefault(nullValue?null:0.0F); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
                break;
            case 6: // Double
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setDoubleNotNullNoDefaultNullValueNone(nullValue?null:0.0D); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        instance.setDoubleNullNoDefaultNullValueNone(nullValue?null:0.0D); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setDoubleNotNullDefaultNullValueNone(nullValue?null:0.0D); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        instance.setDoubleNullDefaultNullValueNone(nullValue?null:0.0D); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setDoubleNotNullNoDefaultNullValueException(nullValue?null:0.0D); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setDoubleNullNoDefaultNullValueException(nullValue?null:0.0D); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setDoubleNotNullDefaultNullValueException(nullValue?null:0.0D); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        instance.setDoubleNullDefaultNullValueException(nullValue?null:0.0D); break;
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setDoubleNotNullNoDefaultNullValueDefault(nullValue?null:0.0D); break;
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setDoubleNullNoDefaultNullValueDefault(nullValue?null:0.0D); break;
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setDoubleNotNullDefaultNullValueDefault(nullValue?null:0.0D); break;
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        instance.setDoubleNullDefaultNullValueDefault(nullValue?null:0.0D); break;
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
                break;
            default:
                throw new IllegalArgumentException("Illegal property type " + propertyType);
        }
    }

    /** Get the value of a specific field.
     *
     * @param instance the instance to get values for
     * @param propertyType the property type
     * @param fieldIndex the field within the property
     */
    private Object getValue(NullValues instance, int propertyType, int fieldIndex) {
        if (instance == null) 
            return null;
        switch(propertyType) {
            case 0: // Integer
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getIntNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getIntNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getIntNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getIntNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getIntNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getIntNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getIntNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getIntNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getIntNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getIntNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getIntNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getIntNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }

            case 1: // Long
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getLongNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getLongNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getLongNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getLongNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getLongNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getLongNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getLongNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getLongNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getLongNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getLongNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getLongNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getLongNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
            case 2: // Short
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getShortNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getShortNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getShortNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getShortNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getShortNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getShortNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getShortNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getShortNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getShortNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getShortNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getShortNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getShortNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
            case 3: // Byte
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getByteNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getByteNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getByteNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getByteNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getByteNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getByteNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getByteNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getByteNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getByteNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getByteNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getByteNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getByteNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }
            case 4: // String
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getStringNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getStringNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getStringNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getStringNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getStringNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getStringNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getStringNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getStringNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getStringNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getStringNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getStringNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getStringNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }

            case 5: // Float
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getFloatNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getFloatNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getFloatNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getFloatNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getFloatNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getFloatNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getFloatNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getFloatNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getFloatNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getFloatNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getFloatNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getFloatNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }

            case 6: // Double
                switch(fieldIndex) {
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getDoubleNotNullNoDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_NONE:
                        return instance.getDoubleNullNoDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getDoubleNotNullDefaultNullValueNone();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_NONE:
                        return instance.getDoubleNullDefaultNullValueNone();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getDoubleNotNullNoDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getDoubleNullNoDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getDoubleNotNullDefaultNullValueException();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_EXCEPTION:
                        return instance.getDoubleNullDefaultNullValueException();
                    case DATABASE_NOT_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getDoubleNotNullNoDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_NO_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getDoubleNullNoDefaultNullValueDefault();
                    case DATABASE_NOT_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getDoubleNotNullDefaultNullValueDefault();
                    case DATABASE_NULL + DATABASE_DEFAULT + NULLVALUE_DEFAULT:
                        return instance.getDoubleNullDefaultNullValueDefault();
                    default:
                        throw new IllegalArgumentException("Illegal field index " + fieldIndex);
                }

            default:
                throw new IllegalArgumentException("Illegal property type " + propertyType);
        }
    }

}
