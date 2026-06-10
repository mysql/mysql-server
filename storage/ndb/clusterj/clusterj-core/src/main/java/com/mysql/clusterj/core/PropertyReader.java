/*
   Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

package com.mysql.clusterj.core;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Constants;

import com.mysql.clusterj.core.util.I18NHelper;

import java.util.Map;

public class PropertyReader implements com.mysql.clusterj.Constants {

    /** My message translator */
    static protected final I18NHelper local = I18NHelper.getInstance(PropertyReader.class);

    /** Get the property from the properties map as a String.
     * @param props the properties
     * @param propertyName the name of the property
     * @return the value from the properties (may be null)
     */
    protected static String getStringProperty(Map<?, ?> props, String propertyName) {
        return (String)props.get(propertyName);
    }

    /** Get the property from the properties map as a String. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static String getStringProperty(Map<?, ?> props, String propertyName, String defaultValue) {
        String result = (String)props.get(propertyName);
        if (result == null) {
            result = defaultValue;
        }
        return result;
    }

    /** Get the property from the properties map as a Boolean.
     */
    static boolean getBooleanProperty(Map<?, ?> props,
                                      String propertyName,
                                      String defaultValue) {
        String result = (String)props.get(propertyName);
        if (result == null) {
            result = defaultValue;
        }
        return Boolean.parseBoolean(result);
    }

    /** Get the property from the properties map as a String. If the user has not
     * provided a value in the props, throw an exception.
     * @param props the properties
     * @param propertyName the name of the property
     * @return the value from the properties (may not be null)
     */
    protected static String getRequiredStringProperty(Map<?, ?> props, String propertyName) {
        String result = (String)props.get(propertyName);
        if (result == null) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NullProperty", propertyName));
        }
        return result;
    }

    /** Get the property from the properties map as an int. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static int getIntProperty(Map<?, ?> props, String propertyName, int defaultValue) {
        Object property = props.get(propertyName);
        if (property == null) {
            return defaultValue;
        }
        if (Number.class.isAssignableFrom(property.getClass())) {
            return ((Number)property).intValue();
        }
        if (property instanceof String) {
            try {
                int result = Integer.parseInt((String)property);
                return result;
            } catch (NumberFormatException ex) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NumericFormat", propertyName, property));
            }
        }
        throw new ClusterJUserException(local.message("ERR_NumericFormat", propertyName, property));
    }

    /** Get the property from the properties map as a long. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static long getLongProperty(Map<?, ?> props, String propertyName, long defaultValue) {
        Object property = props.get(propertyName);
        if (property == null) {
            return defaultValue;
        }
        if (Number.class.isAssignableFrom(property.getClass())) {
            return ((Number)property).longValue();
        }
        if (property instanceof String) {
            try {
                long result = Long.parseLong((String)property);
                return result;
            } catch (NumberFormatException ex) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NumericFormat", propertyName, property));
            }
        }
        throw new ClusterJUserException(local.message("ERR_NumericFormat", propertyName, property));
    }
}


