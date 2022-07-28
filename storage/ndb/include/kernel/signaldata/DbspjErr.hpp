
/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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
