/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DBSPJ_ERR_H
#define DBSPJ_ERR_H

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
  };
};

#endif
