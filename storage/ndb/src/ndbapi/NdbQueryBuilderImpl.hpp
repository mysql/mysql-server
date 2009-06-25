/*
   Copyright (C) 2009 Sun Microsystems Inc
    All rights reserved. Use is subject to license terms.

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


#ifndef NdbQueryBuilderImpl_H
#define NdbQueryBuilderImpl_H


#include <Vector.hpp>

class NdbQueryOperationDefImpl;
class NdbParamOperandImpl;
class NdbConstOperandImpl;
class NdbLinkedOperandImpl;


class NdbQueryBuilderImpl
{
  friend class NdbQueryBuilder;

public:
  ~NdbQueryBuilderImpl();
  NdbQueryBuilderImpl(Ndb& ndb);

  class NdbQueryDef* prepare();

  const NdbError& getNdbError() const;

  void setErrorCode(int aErrorCode)
  { if (!m_error.code)
      m_error.code = aErrorCode;
  }

private:
  bool hasError() const
  { return (m_error.code!=0); }

  bool contains(const NdbQueryOperationDefImpl*);

  Ndb& m_ndb;
  NdbError m_error;

  Vector<NdbQueryOperationDefImpl*> m_operation;
  Vector<NdbParamOperandImpl*> m_paramOperand;
  Vector<NdbConstOperandImpl*> m_constOperand;
  Vector<NdbLinkedOperandImpl*> m_linkedOperand;

}; // class NdbQueryBuilderImpl



#endif
