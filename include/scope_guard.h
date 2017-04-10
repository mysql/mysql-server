/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef SCOPE_GUARD_H
#define SCOPE_GUARD_H

template <typename TLambda>
class Scope_guard
{
public:
  Scope_guard(const TLambda& rollback_lambda)
    : m_committed(false),
    m_rollback_lambda(rollback_lambda)
  {
  }
  Scope_guard(const Scope_guard<TLambda>&)= delete;
  Scope_guard(Scope_guard<TLambda>&& moved)
    : m_committed(moved.m_committed),
    m_rollback_lambda(moved.m_rollback_lambda)
  {
    /* Set moved guard to "invalid" state, the one in which the rollback lambda
      will not be executed. */
    moved.m_committed= true;
  }
  ~Scope_guard()
  {
    if (!m_committed)
    {
      m_rollback_lambda();
    }
  }

  inline void commit()
  {
    m_committed = true;
  }

private:
  bool m_committed;
  const TLambda m_rollback_lambda;
};

template <typename TLambda>
Scope_guard<TLambda> create_scope_guard(const TLambda rollback_lambda)
{
  return Scope_guard<TLambda>(rollback_lambda);
}

#endif /* SCOPE_GUARD_H */
