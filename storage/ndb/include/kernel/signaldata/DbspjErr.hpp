
/*
   Copyright (c) 2004, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DBSPJ_ERR_H
#define DBSPJ_ERR_H

#define JAM_FILE_ID 155


struct DbspjErr
{
  enum {
    OutOfOperations = 20000
    ,ZeroLengthQueryTree = 20001
    ,InvalidRequest = 20002
    ,UnknowQueryOperation = 20003
    ,InvalidTreeNodeSpecification = 20004
    ,InvalidTreeParametersSpecification = 20005
    ,OutOfSectionMemory = 20006
    ,InvalidPattern = 20007
    ,OutOfQueryMemory = 20008
    ,QueryNodeTooBig = 20009
    ,QueryNodeParametersTooBig = 20010
    ,BothTreeAndParametersContainInterpretedProgram = 20011
    ,InvalidTreeParametersSpecificationKeyParamBitsMissmatch = 20012
    ,InvalidTreeParametersSpecificationIncorrectKeyParamCount = 20013
    ,InternalError = 20014
    ,OutOfRowMemory = 20015
    ,NodeFailure = 20016
    ,InvalidTreeNodeCount = 20017
    ,IndexFragNotFound = 20018
    ,NoSuchTable = 20019
    ,DropTableInProgress = 20020
    ,WrongSchemaVersion = 20021
  };
};


#undef JAM_FILE_ID

#endif
