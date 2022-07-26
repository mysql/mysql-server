/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

package com.mysql.clusterj;

/**
 * ClusterJUserException represents a database error. The underlying cause
 * of the exception is contained in the "cause".
 */
public class ClusterJDatastoreException extends ClusterJException {

    private static final long serialVersionUID = 2208896230646592560L;

    private int code = 0;

    /** Get the code
     @since 7.3.15, 7.4.13, 7.5.4
    */
    public int getCode() {
        return code;
    }

    private int mysqlCode = 0;

    /** Get the mysql code
     @since 7.3.15, 7.4.13, 7.5.4
     */
    public int getMysqlCode() {
        return mysqlCode;
    }

    private int status = 0;

    /** Get the status
     */
    public int getStatus() {
        return status;
    }

    private int classification = 0;

    /** Get the classification
     */
    public int getClassification() {
        return classification;
    }

    public ClusterJDatastoreException(String message) {
        super(message);
    }

    public ClusterJDatastoreException(String message, Throwable t) {
        super(message, t);
    }

    public ClusterJDatastoreException(Throwable t) {
        super(t);
    }

    public ClusterJDatastoreException(String msg, int code, int mysqlCode,
            int status, int classification) {
        super(msg);
        this.code = code;
        this.mysqlCode = mysqlCode;
        this.status = status;
        this.classification = classification;
    }

    /** Helper class for getClassification().
     * import com.mysql.clusterj.ClusterJDatastoreException.Classification;
     * Classification c = Classification.lookup(datastoreException.getClassification());
     * System.out.println("exceptionClassification " + c + " with value " + c.value);
     * @since 7.3.15, 7.4.13, 7.5.4
     */
    public enum Classification {
        NoError                ( 0 ) /*_ndberror_cl_none_*/,
        ApplicationError       ( 1 ) /*_ndberror_cl_application_*/,
        NoDataFound            ( 2 ) /*_ndberror_cl_no_data_found_*/,
        ConstraintViolation    ( 3 ) /*_ndberror_cl_constraint_violation_*/,
        SchemaError            ( 4 ) /*_ndberror_cl_schema_error_*/,
        UserDefinedError       ( 5 ) /*_ndberror_cl_user_defined_*/,
        InsufficientSpace      ( 6 ) /*_ndberror_cl_insufficient_space_*/,
        TemporaryResourceError ( 7 ) /*_ndberror_cl_temporary_resource_*/,
        NodeRecoveryError      ( 8 ) /*_ndberror_cl_node_recovery_*/,
        OverloadError          ( 9 ) /*_ndberror_cl_overload_*/,
        TimeoutExpired         ( 10 ) /*_ndberror_cl_timeout_expired_*/,
        UnknownResultError     ( 11 ) /*_ndberror_cl_unknown_result_*/,
        InternalError          ( 12 ) /*_ndberror_cl_internal_error_*/,
        FunctionNotImplemented ( 13 ) /*_ndberror_cl_function_not_implemented_*/,
        UnknownErrorCode       ( 14 ) /*_ndberror_cl_unknown_error_code_*/,
        NodeShutdown           ( 15 ) /*_ndberror_cl_node_shutdown_*/,
        SchemaObjectExists     ( 17 ) /*_ndberror_cl_schema_object_already_exists_*/,
        InternalTemporary      ( 18 ) /*_ndberror_cl_internal_temporary_*/;

        Classification(int value) {
            this.value = value;
        }

        public final int value;

        private static Classification[] entries = new Classification[] {
            NoError, ApplicationError, NoDataFound, ConstraintViolation, SchemaError,
            UserDefinedError,InsufficientSpace, TemporaryResourceError, NodeRecoveryError,
            OverloadError, TimeoutExpired, UnknownResultError, InternalError,
            FunctionNotImplemented, UnknownErrorCode, NodeShutdown, null,
            SchemaObjectExists, InternalTemporary};

        /** Get the Classification enum for a value returned by
         * ClusterJDatastoreException.getClassification().
         * @param value the classification returned by getClassification()
         * @return the Classification for the error
         */
        public static Classification lookup(int value) {
            return (value >= 0) && (value < entries.length) ? entries[value] : null;
        }
    }

}
