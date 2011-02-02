/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;



import java.sql.SQLException;
import java.sql.Types;
import java.util.BitSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import org.apache.openjpa.jdbc.kernel.JDBCStore;
import org.apache.openjpa.jdbc.meta.JavaSQLTypes;
import org.apache.openjpa.jdbc.schema.Column;
import org.apache.openjpa.jdbc.sql.AbstractResult;
import org.apache.openjpa.jdbc.sql.Joins;
import org.apache.openjpa.jdbc.sql.Result;
import org.apache.openjpa.meta.JavaTypes;


/**
 * Result for use with the Ndb runtime.
 *
 */
public class NdbOpenJPAResult extends AbstractResult implements Result {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPAResult.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPAResult.class);

    /** The ResultData as returned from the operation. */
    protected ResultData resultData;

    /** The DomainTypeHandler for the root object of the operation. */
    protected DomainTypeHandler<?> domainTypeHandler;

    /** The BitSet of fields requested from the operation. */
    protected BitSet fields;

    /** The Set of columns in this result. */
    // TODO: needed? might need if joining tables with the same column names
//    protected Set<Column> columns = new HashSet<Column>();

    /** The Map of column names to columns in this result. */
    private Map<String, com.mysql.clusterj.core.store.Column> columnMap =
        new HashMap<String, com.mysql.clusterj.core.store.Column>();

    /**
     * Construct the Result with the result of an NDB query.
     * This encapsulates both the result from the database and the
     * metadata that describes the result.
     */
    public NdbOpenJPAResult(ResultData resultData, DomainTypeHandler<?> domainTypeHandler,
            BitSet fields) {
        super();
        this.resultData = resultData;
        this.domainTypeHandler = domainTypeHandler;
        this.fields = fields;
        Set<com.mysql.clusterj.core.store.Column> storeColumns = domainTypeHandler.getStoreColumns(fields);
        for (com.mysql.clusterj.core.store.Column storeColumn: storeColumns) {
            columnMap.put(storeColumn.getName(), storeColumn);
        }
    }

    @Override
    protected boolean nextInternal() throws SQLException {
        boolean result = resultData.next();
        if (logger.isDetailEnabled()) {
            logger.detail("returning: " + result);
        }
        return result;
    }

    @Override
    protected boolean containsInternal(Object obj, Joins joins) throws SQLException {
        // TODO: support join specifications
        com.mysql.clusterj.core.store.Column storeColumn = resolve(obj);
        return storeColumn != null;
    }

    @Override
    protected Object getObjectInternal(Object obj, int metaType, Object arg, Joins joins) throws SQLException {
        // TODO: support other object types
        // TODO: support null values for int, double, float, long, and short
        Object result = null;
        com.mysql.clusterj.core.store.Column columnName = resolve(obj);
        switch (metaType) {
            case JavaTypes.INT_OBJ:
//                int value = resultData.getInt(columnName);
//                result = resultData.wasNull(columnName)?null:value;
            case JavaTypes.INT:
                result = resultData.getInt(columnName);
                break;
            case JavaTypes.DOUBLE_OBJ:
//              double value = resultData.getDouble(columnName);
//              result = resultData.wasNull(columnName)?null:value;
            case JavaTypes.DOUBLE:
                result = resultData.getDouble(columnName);
                break;
            case JavaTypes.FLOAT_OBJ:
//              float value = resultData.getFloat(columnName);
//              result = resultData.wasNull(columnName)?null:value;
            case JavaTypes.FLOAT:
                result = resultData.getFloat(columnName);
                break;
            case JavaTypes.STRING:
                result = resultData.getString(columnName);
                break;
            case JavaTypes.LONG_OBJ:
//              long value = resultData.getLong(columnName);
//              result = resultData.wasNull(columnName)?null:value;
            case JavaTypes.LONG:
                result = resultData.getLong(columnName);
                break;
            case JavaTypes.BIGDECIMAL:
                result = resultData.getDecimal(columnName);
                break;
            case JavaTypes.BIGINTEGER:
                result = resultData.getBigInteger(columnName);
                break;
            case JavaTypes.DATE:
                result = new java.util.Date(resultData.getLong(columnName));
                break;
            case JavaSQLTypes.TIME:
                result = new java.sql.Time(resultData.getLong(columnName));
                break;
            case JavaSQLTypes.SQL_DATE:
                result = new java.sql.Date(resultData.getLong(columnName));
                break;
            case JavaSQLTypes.TIMESTAMP:
                result = new java.sql.Timestamp(resultData.getLong(columnName));
                break;
            case JavaSQLTypes.BYTES:
                result = resultData.getBytes(columnName);
                break;
            case JavaTypes.BOOLEAN:
            case JavaTypes.BOOLEAN_OBJ:
                result = resultData.getObjectBoolean(columnName);
                break;
            default:
                if (obj instanceof Column) {
                    Column col = (Column) obj;
                    if (col.getType() == Types.BLOB
                        || col.getType() == Types.VARBINARY) {
                        result = resultData.getBytes(columnName);
                    }
                }
                result = resultData.getInt(columnName);
        }
        if (logger.isDetailEnabled()) {
            logger.detail("obj: " + obj + " arg: " + arg + " joins: " + joins + " metaType: " + metaType + " result: " + result);
        }
        return result;
    }

    @Override
    protected Object getStreamInternal(JDBCStore store, Object obj, int metaType, Object arg, Joins joins) throws SQLException {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public int size() throws SQLException {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    /** This method exists solely to bypass a known bug in OpenJPA.
     * http://issues.apache.org/jira/browse/OPENJPA-1158
     * @param obj the column (or column name) to fetch
     * @param joins joins
     * @return the long value
     * @throws java.sql.SQLException 
     */
    @Override
    protected long getLongInternal(Object obj, Joins joins)
        throws SQLException {
        Number val = (Number) checkNull(getObjectInternal(obj,
            JavaTypes.LONG, null, joins));
        return (val == null) ? 0 : val.longValue();
    }

    protected com.mysql.clusterj.core.store.Column resolve(Object obj) {
        String key = null;
        com.mysql.clusterj.core.store.Column result = null;
        if (logger.isDetailEnabled()) {
            logger.detail("resolving object of type: " + obj.getClass().getName());
        }
        if (obj instanceof String) {
            key = (String)obj;
            result = columnMap.get(key);
        } else if (obj instanceof Column) {
            key = ((Column)obj).getName();
            result = columnMap.get(key);
        } else {
            throw new UnsupportedOperationException(
                    local.message("ERR_Unsupported_Object_Type_For_Resolve", obj.getClass().getName()));
        }
        if (logger.isDetailEnabled()) logger.detail("key: " + key + " column: " + ((result==null)?"<null>":result.getName()));
        return result;
    }

    public Set<String> getColumnNames() {
        return columnMap.keySet();
    }
}
