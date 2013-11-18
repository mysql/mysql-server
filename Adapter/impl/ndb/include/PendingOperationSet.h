/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

#ifndef nodejs_adapter_PendingOperationSet_h
#define nodejs_adapter_PendingOperationSet_h

class NdbOperation;

class PendingOperationSet {
public:
  PendingOperationSet(int size);
  ~PendingOperationSet();
  void setNdbOperation(int n, const NdbOperation *op);
  void setError(int n, const NdbError &);
  const NdbError * getOperationError(int n);

private:
  int size;
  const NdbOperation ** const ops;
  const NdbError ** const errors;
};

inline PendingOperationSet::PendingOperationSet(int _sz) :
  size(_sz),
  ops(new const NdbOperation *[_sz]),
  errors(new const NdbError *[_sz])                    {};

inline PendingOperationSet::~PendingOperationSet() {
  delete[] ops;
  delete[] errors;
}

inline void PendingOperationSet::setNdbOperation(int n, const NdbOperation *op) {
  if(n < size) {
    ops[n] = op;
    errors[n] = NULL;
  }
}

inline void PendingOperationSet::setError(int n, const NdbError & err) {
  if(n < size) {
    errors[n] = & err;
    ops[n] = NULL;
  }
}

inline const NdbError * PendingOperationSet::getOperationError(int n) {
  assert(n < size);
  return (ops[n] ? & ops[n]->getNdbError() : errors[n]);
}

#endif
