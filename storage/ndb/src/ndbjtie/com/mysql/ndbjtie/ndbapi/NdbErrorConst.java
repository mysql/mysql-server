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
 * NdbErrorConst.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public interface /*_struct_*/ NdbErrorConst
{
    public interface /*_enum_*/ Status
    {
        int Success = 0 /*_ndberror_st_success_*/,
            TemporaryError = 1 /*_ndberror_st_temporary_*/,
            PermanentError = 2 /*_ndberror_st_permanent_*/,
            UnknownResult = 3 /*_ndberror_st_unknown_*/;
    }
    public interface /*_enum_*/ Classification
    {
        int NoError = 0 /*_ndberror_cl_none_*/,
            ApplicationError = 1 /*_ndberror_cl_application_*/,
            NoDataFound = 2 /*_ndberror_cl_no_data_found_*/,
            ConstraintViolation = 3 /*_ndberror_cl_constraint_violation_*/,
            SchemaError = 4 /*_ndberror_cl_schema_error_*/,
            UserDefinedError = 5 /*_ndberror_cl_user_defined_*/,
            InsufficientSpace = 6 /*_ndberror_cl_insufficient_space_*/,
            TemporaryResourceError = 7 /*_ndberror_cl_temporary_resource_*/,
            NodeRecoveryError = 8 /*_ndberror_cl_node_recovery_*/,
            OverloadError = 9 /*_ndberror_cl_overload_*/,
            TimeoutExpired = 10 /*_ndberror_cl_timeout_expired_*/,
            UnknownResultError = 11 /*_ndberror_cl_unknown_result_*/,
            InternalError = 12 /*_ndberror_cl_internal_error_*/,
            FunctionNotImplemented = 13 /*_ndberror_cl_function_not_implemented_*/,
            UnknownErrorCode = 14 /*_ndberror_cl_unknown_error_code_*/,
            NodeShutdown = 15 /*_ndberror_cl_node_shutdown_*/,
            SchemaObjectExists = 17 /*_ndberror_cl_schema_object_already_exists_*/,
            InternalTemporary = 18 /*_ndberror_cl_internal_temporary_*/;
    }
    int/*_Status_*/ status();
    int/*_Classification_*/ classification();
    int code();
    int mysql_code();
    String/*_const char *_*/ message();
}
