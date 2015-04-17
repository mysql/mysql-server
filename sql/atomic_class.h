/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */


#ifndef ATOMIC_CLASS_H_INCLUDED
#define ATOMIC_CLASS_H_INCLUDED

#include "my_atomic.h"

/**
  Wrapper class to use C++ syntax to access atomic integers.
*/
#define DEFINE_ATOMIC_CLASS(NAME, SUFFIX, TYPE)                         \
  class Atomic_##NAME                                                   \
  {                                                                     \
public:                                                                 \
    /* Create a new Atomic_* object. */                                 \
    Atomic_##NAME(TYPE n= 0)                                            \
    {                                                                   \
      my_atomic_store##SUFFIX(&value, n);                               \
    }                                                                   \
    /* Atomically read the value. */                                    \
    TYPE atomic_get()                                                   \
    {                                                                   \
      return my_atomic_load##SUFFIX(&value);                            \
    }                                                                   \
    /* Atomically set the value. */                                     \
    void atomic_set(TYPE n)                                             \
    {                                                                   \
      my_atomic_store##SUFFIX(&value, n);                               \
    }                                                                   \
    /* Atomically add to the value, and return the old value. */        \
    TYPE atomic_add(TYPE n)                                             \
    {                                                                   \
      return my_atomic_add##SUFFIX(&value, n);                          \
    }                                                                   \
    /* Atomically set the value and return the old value. */            \
    TYPE atomic_get_and_set(TYPE n)                                     \
    {                                                                   \
      return my_atomic_fas##SUFFIX(&value, n);                          \
    }                                                                   \
    /* If the old value is equal to *old, set the value to new and */   \
    /* return true. Otherwise, store the old value in '*old' and */     \
    /* return false. */                                                 \
    bool atomic_compare_and_swap(TYPE *old, TYPE n)                     \
    {                                                                   \
      return my_atomic_cas##SUFFIX(&value, old, n);                     \
    }                                                                   \
    /* Read the value *non-atomically*. */                              \
    TYPE non_atomic_get()                                               \
    {                                                                   \
      return value;                                                     \
    }                                                                   \
    /* Set the value *non-atomically*. */                               \
    void non_atomic_set(TYPE n)                                         \
    {                                                                   \
      value= n;                                                         \
    }                                                                   \
    /* Add to the value *non-atomically*. */                            \
    TYPE non_atomic_add(TYPE n)                                         \
    {                                                                   \
      TYPE ret= value;                                                  \
      value+= n;                                                        \
      return ret;                                                       \
    }                                                                   \
    /* Set the value to the greatest of (old, n). */                    \
                                                                        \
    /* The function will internally requires multiple atomic */         \
    /* operations.  If the old value is known (or guessed), and less */ \
    /* than n, it requires one atomic operation less. Therefore, */     \
    /* the caller should set *guess to whatever is the likely value */  \
    /* that the variable currently has, if such a guess is known. */    \
                                                                        \
    /* @return If the new value is changed to n, *guess is set to */    \
    /* the old value and the the function returns true.  Otherwise, */  \
    /* *guess is set to the current value (which is greater than or */  \
    /* equal to n), and the function returns false. */                  \
    bool atomic_set_to_max(TYPE n, TYPE *guess= NULL)                   \
    {                                                                   \
      TYPE _guess;                                                      \
      if (guess == NULL)                                                \
      {                                                                 \
        _guess= n - 1;                                                  \
        guess= &_guess;                                                 \
      }                                                                 \
      else                                                              \
        DBUG_ASSERT(*guess < n);                                        \
      bool ret;                                                         \
      do {                                                              \
        ret= atomic_compare_and_swap(guess, n);                         \
      } while (!ret && *guess < n);                                     \
      return ret;                                                       \
    }                                                                   \
                                                                        \
private:                                                                \
    TYPE value;                                                         \
  }

DEFINE_ATOMIC_CLASS(int32, 32, int32);
DEFINE_ATOMIC_CLASS(int64, 64, int64);
//DEFINE_ATOMIC_CLASS(pointer, ptr, void *);

#endif //ifndef ATOMIC_CLASS_H_INCLUDED
