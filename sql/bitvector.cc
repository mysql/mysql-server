/* -*- Mode: C++ -*-

   Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include <bitvector.h>

void bitvector::create_last_word_mask()
{

  /* Get the number of used bits (1..8) in the last byte */
  unsigned int const used= 1U + ((size()-1U) & 0x7U);

  /*
   * Create a mask with the upper 'unused' bits set and the lower 'used'
   * bits clear. The bits within each byte is stored in big-endian order.
   */
  unsigned char const mask= (~((1 << used) - 1)) & 255;
  unsigned int byte_no= ((bytes()-1)) & ~3U;
  last_word_ptr= (uint32*)&m_data[byte_no];

  /*
    The first bytes are to be set to zero since they represent real  bits
    in the bitvector. The last bytes are set to 0xFF since they  represent
    bytes not used by the bitvector. Finally the last byte contains  bits
    as set by the mask above.
  */

  unsigned char *ptr= (unsigned char*)&last_word_mask;
  switch (bytes()&3)
  {
  case 1:
    last_word_mask= ~0U;
    ptr[0]= mask;
    return;
  case 2:
    last_word_mask= ~0U;
    ptr[0]= 0;
    ptr[1]= mask;
    return;
  case 3:
    last_word_mask= 0U;
    ptr[2]= mask;
    ptr[3]= 0xFFU;
    return;
  case 0:
    last_word_mask= 0U;
    ptr[3]= mask;
    return;
  }
}

int bitvector::init(size_t size)
{
  DBUG_ASSERT(size < MYSQL_NO_BIT_FOUND);
  DBUG_ASSERT(size > 0);
  m_size= size;
  m_data= (uchar*)sql_alloc(byte_size_word_aligned(size));
  if (m_data)
  {
    create_last_word_mask();
    clear_all();
    return FALSE;
  }
  return TRUE;
}

uint bitvector::no_bits_set()
{
  uint no_bytes= bytes(), res=0, i;
  uchar *ptr= m_data;
  *last_word_ptr^=last_word_mask; //Reset last bits to zero
  for (i=0; i< no_bytes; i++, ptr++)
    res+=my_count_bits_ushort(*ptr);
  *last_word_ptr^=last_word_mask; //Set last bits to one again
  return res;
}

uint bitvector::get_first_bit_set()
{
  uchar *byte_ptr;
  uint32 *data_ptr= (uint32*)data(), bit_found,i,j,k;
  for (i=0; data_ptr <= last_word_ptr; data_ptr++, i++)
  {
    if (*data_ptr)
    {
      byte_ptr= (uchar*)data_ptr;
      for (j=0; j < 4; j++, byte_ptr++)
      {
        if (*byte_ptr)
        {
          for (k=0; k < 8; k++)
          {
            if (*byte_ptr & (1 << k))
            {
              bit_found= (i << 5) + (j << 3) + k;
              if (bit_found == m_size)
                return MYSQL_NO_BIT_FOUND;
              else
                return bit_found;
            }
          }
          DBUG_ASSERT(1);
        }
      }
      DBUG_ASSERT(1);
    }
  }
  return MYSQL_NO_BIT_FOUND;
}

uint bitvector::get_first_bit_clear()
{
  uchar *byte_ptr;
  uint32 *data_ptr= (uint32*)data(), bit_found,i,j,k;
  for (i=0; data_ptr <= last_word_ptr; data_ptr++, i++)
  {
    if (*data_ptr != 0xFFFFFFFF)
    {
      byte_ptr= (uchar*)data_ptr;
      for (j=0; j < 4; j++, byte_ptr++)
      {
        if (*byte_ptr != 0xFF)
        {
          for (k=0; k < 8; k++)
          {
            if (!(*byte_ptr & (1 << k)))
            {
              bit_found= (i << 5) + (j << 3) + k;
              if (bit_found == m_size)
                return MYSQL_NO_BIT_FOUND;
              else
                return bit_found;
            }
          }
          DBUG_ASSERT(1);
        }
      }
      DBUG_ASSERT(1);
    }
  }
  return MYSQL_NO_BIT_FOUND;
}

#ifdef TEST_BITVECTOR
uint get_rand_bit(uint bitsize)
{
  return (rand() % bitsize);
}

bool test_set_get_clear_bit(bitvector *bv, uint bitsize)
{
  uint i, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit= get_rand_bit(bitsize);
    bv->set_bit(test_bit);
    if (!bv->get_bit(test_bit))
      goto error1;
    bv->clear_bit(test_bit);
    if (bv->get_bit(test_bit))
      goto error2;
  }
  return FALSE;
error1:
  printf("Error in set bit, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
error2:
  printf("Error in clear bit, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
}

bool test_flip_bit(bitvector *bv, uint bitsize)
{
  uint i, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit= get_rand_bit(bitsize);
    bv->flip_bit(test_bit);
    if (!bv->get_bit(test_bit))
      goto error1;
    bv->flip_bit(test_bit);
    if (bv->get_bit(test_bit))
      goto error2;
  }
  return FALSE;
error1:
  printf("Error in flip bit 1, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
error2:
  printf("Error in flip bit 2, bit %u, bitsize = %u", test_bit, bitsize);
  return TRUE;
}

bool test_operators(bitvector *bv, uint bitsize)
{
  return FALSE;
}

bool test_get_all_bits(bitvector *bv, uint bitsize)
{
  uint i;
  bv->set_all();
  if (!bv->get_all_bits_set())
    goto error1;
  bv->clear_all();
  if (!bv->get_all_bits_clear())
    goto error2;
  for (i=0; i<bitsize;i++)
    bv->set_bit(i);
  if (!bv->get_all_bits_set())
    goto error3;
  for (i=0; i<bitsize;i++)
    bv->clear_bit(i);
  if (!bv->get_all_bits_clear())
    goto error4;
  return FALSE;
error1:
  printf("Error in set_all, bitsize = %u", bitsize);
  return TRUE;
error2:
  printf("Error in clear_all, bitsize = %u", bitsize);
  return TRUE;
error3:
  printf("Error in bitwise set all, bitsize = %u", bitsize);
  return TRUE;
error4:
  printf("Error in bitwise clear all, bitsize = %u", bitsize);
  return TRUE;
}

bool test_compare_operators(bitvector *bv, uint bitsize)
{
  return FALSE;
}

bool test_count_bits_set(bitvector *bv, uint bitsize)
{
  uint i, bit_count=0, test_bit;
  uint no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    if (!bv->get_bit(test_bit))
    {
      bv->set_bit(test_bit);
      bit_count++;
    }
  }
  if (bit_count==0 && bitsize > 0)
    goto error1;
  if (bv->no_bits_set() != bit_count)
    goto error2;
  return FALSE;
error1:
  printf("No bits set  bitsize = %u", bitsize);
  return TRUE;
error2:
  printf("Wrong count of bits set, bitsize = %u", bitsize);
  return TRUE;
}

bool test_get_first_bit(bitvector *bv, uint bitsize)
{
  return FALSE;
}

bool test_get_next_bit(bitvector *bv, uint bitsize)
{
  return FALSE;
}

bool do_test(uint bitsize)
{
  bitvector *bv;
  bv = new bitvector;
  bv->init(bitsize);
  if (test_set_get_clear_bit(bv,bitsize))
    goto error;
  bv->clear_all();
  if (test_flip_bit(bv,bitsize))
    goto error;
  bv->clear_all();
  if (test_operators(bv,bitsize))
    goto error;
  bv->clear_all();
  if (test_get_all_bits(bv, bitsize))
    goto error;
  bv->clear_all();
  if (test_compare_operators(bv,bitsize))
    goto error;
  bv->clear_all();
  if (test_count_bits_set(bv,bitsize))
    goto error;
  bv->clear_all();
  if (test_get_first_bit(bv,bitsize))
    goto error;
  bv->clear_all();
  if (test_get_next_bit(bv,bitsize))
    goto error;
  delete bv;
  return FALSE;
error:
  delete bv;
  printf("\n");
  return TRUE;
}

int main()
{
  int i;
  for (i= 1; i < 4096; i++)
    if (do_test(i))
      return -1;
  printf("OK\n");
  return 0;
}
/*
Compile by using the below on a compiled clone

g++ -DHAVE_CONFIG_H -I. -I. -I.. -I../include -I../regex -I.  -I../include
-g -fno-omit-frame-pointer -fno-common -felide-constructors -fno-exceptions
-fno-rtti   -fno-implicit-templates -fno-exceptions -fno-rtti
-DUSE_MYSYS_NEW -DDEFINE_CXA_PURE_VIRTUAL -DHAVE_DARWIN_THREADS
-D_P1003_1B_VISIBLE -DTEST_BITVECTOR -DSIGNAL_WITH_VIO_CLOSE
-DSIGNALS_DONT_BREAK_READ -DIGNORE_SIGHUP_SIGQUIT  -o bitvector.o
-c bitvector.cc
g++ -o bitvector bitvector.o -L../mysys -lmysys -L../dbug -L../strings
-lmystrings -ldbug
*/
#endif
