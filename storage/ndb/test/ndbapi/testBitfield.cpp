/*
   Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <HugoTransactions.hpp>
#include <Bitmask.hpp>
#include <Vector.hpp>
#include "my_alloc.h"

static const char* _dbname = "TEST_DB";
static int g_loops = 7;

struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const NdbDictionary::Table* create_random_table(Ndb*);
static int transactions(Ndb*, const NdbDictionary::Table* tab);
static int unique_indexes(Ndb*, const NdbDictionary::Table* tab);
static int ordered_indexes(Ndb*, const NdbDictionary::Table* tab);
static int node_restart(Ndb*, const NdbDictionary::Table* tab);
static int system_restart(Ndb*, const NdbDictionary::Table* tab);
static int testBitmask();

int 
main(int argc, char** argv){
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);

  if (opts.handle_options())
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  int res = NDBT_FAILED;

  /* Run cluster-independent tests */
  for (int i=0; i<(10*g_loops); i++)
  {
    if (NDBT_OK != (res= testBitmask()))
      return NDBT_ProgramExit(res);
  }
  
  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  if(con.connect(12, 5, 1))
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  

  Ndb* pNdb;
  pNdb = new Ndb(&con, _dbname);  
  pNdb->init();
  while (pNdb->waitUntilReady() != 0) {};

  NdbDictionary::Dictionary * dict = pNdb->getDictionary();

  const NdbDictionary::Table* pTab = 0;
  for (int i = 0; i < (argc ? argc : g_loops) ; i++)
  {
    res = NDBT_FAILED;
    if(argc == 0)
    {
      pTab = create_random_table(pNdb);
    }
    else
    {
      dict->dropTable(argv[i]);
      NDBT_Tables::createTable(pNdb, argv[i]);
      pTab = dict->getTable(argv[i]);
    }
    
    if (pTab == 0)
    {
      ndbout << "Failed to create table" << endl;
      ndbout << dict->getNdbError() << endl;
      break;
    }
    
    if(transactions(pNdb, pTab))
      break;

    if(unique_indexes(pNdb, pTab))
      break;

    if(ordered_indexes(pNdb, pTab))
      break;
    
    if(node_restart(pNdb, pTab))
      break;
    
    if(system_restart(pNdb, pTab))
      break;

    dict->dropTable(pTab->getName());
    res = NDBT_OK;
  }

  if(res != NDBT_OK && pTab)
  {
    dict->dropTable(pTab->getName());
  }
  
  delete pNdb;
  return NDBT_ProgramExit(res);
}

static 
const NdbDictionary::Table* 
create_random_table(Ndb* pNdb)
{
  do {
    NdbDictionary::Table tab;

    // Table need as minimum a PK and an 'Update count' column
    Uint32 cols = 2 + (rand() % (NDB_MAX_ATTRIBUTES_IN_TABLE - 2));
    const Uint32 maxLength = 4090;
    Uint32 length = maxLength;
    Uint8  defbuf[(maxLength + 7)/8];
    
    BaseString name; 
    name.assfmt("TAB_%d", rand() & 65535);
    tab.setName(name.c_str());
    for(Uint32 i = 0; i<cols && length > 2; i++)
    {
      NdbDictionary::Column col;
      name.assfmt("COL_%d", i);
      col.setName(name.c_str());
      if(i == 0 || i == 1)
      {
	col.setType(NdbDictionary::Column::Unsigned);
	col.setLength(1); 
	col.setNullable(false);
	col.setPrimaryKey(i == 0);
	tab.addColumn(col);
	continue;
      }
      
      col.setType(NdbDictionary::Column::Bit);
      
      Uint32 len = 1 + (rand() % (length - 1));
      memset(defbuf, 0, (length + 7)/8);
      for (Uint32 j = 0; j < len/8; j++)
        defbuf[j] = 0x63;
      col.setDefaultValue(defbuf, 4*((len + 31)/32));
      col.setLength(len); length -= len;
      int nullable = (rand() >> 16) & 1;
      col.setNullable(nullable); length -= nullable;
      col.setPrimaryKey(false);
      tab.addColumn(col);
    }
    
    pNdb->getDictionary()->dropTable(tab.getName());
    if(pNdb->getDictionary()->createTable(tab) == 0)
    {
      ndbout << (NDBT_Table&)tab << endl;
      return pNdb->getDictionary()->getTable(tab.getName());
    }
  } while(0);
  return 0;
}

static 
int
transactions(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  int i = 0;
  HugoTransactions trans(* tab);
  i |= trans.loadTable(pNdb, 1000);
  i |= trans.pkReadRecords(pNdb, 1000, 13); 
  i |= trans.scanReadRecords(pNdb, 1000, 25);
  i |= trans.pkUpdateRecords(pNdb, 1000, 37);
  i |= trans.scanUpdateRecords(pNdb, 1000, 25);
  i |= trans.pkDelRecords(pNdb, 500, 23);
  i |= trans.clearTable(pNdb);
  return i;
}

static 
int 
unique_indexes(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

static 
int 
ordered_indexes(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

static 
int 
node_restart(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

static 
int 
system_restart(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

/* Note : folowing classes test functionality of storage/ndb/src/common/util/Bitmask.cpp
 * and were originally defined there.
 * Set BITMASK_DEBUG to 1 to get more test debugging info.
 */
#define BITMASK_DEBUG 0

static
bool cmp(const Uint32 b1[], const Uint32 b2[], Uint32 len)
{
  Uint32 sz32 = (len + 31) >> 5;
  for(Uint32 i = 0; i<len; i++)
  {
    if(BitmaskImpl::get(sz32, b1, i) ^ BitmaskImpl::get(sz32, b2, i))
      return false;
  }
  return true;
}

static
void print(const Uint32 src[], Uint32 len, Uint32 pos = 0)
{
  printf("b'");
  for(unsigned i = 0; i<len; i++)
  {
    if(BitmaskImpl::get((pos + len + 31) >> 5, src, i+pos))
      printf("1");
    else
      printf("0");
    if((i & 31) == 31)
      printf(" ");
  }
}

static int lrand()
{
  return rand();
}

static
void rand(Uint32 dst[], Uint32 len)
{
  for(Uint32 i = 0; i<len; i++)
    BitmaskImpl::set((len + 31) >> 5, dst, i, (lrand() % 1000) > 500);
}

static
int checkCopyField(const Uint32 totalTests)
{
  ndbout << "Testing : Checking Bitmaskimpl::copyField"
         << endl;

  const Uint32 numWords= 95;
  const Uint32 maxBitsToCopy= (numWords * 32);
  
  Uint32 sourceBuf[numWords];
  Uint32 targetTest[numWords];
  Uint32 targetCopy[numWords];

  rand(sourceBuf, maxBitsToCopy);
  
  /* Set both target buffers to the same random values */
  rand(targetTest, maxBitsToCopy);
  for (Uint32 i=0; i<maxBitsToCopy; i++)
    BitmaskImpl::set(numWords, targetCopy, i, 
                     BitmaskImpl::get(numWords, targetTest, i));

  if (!cmp(targetTest, targetCopy, maxBitsToCopy))
  {
    ndbout_c("copyField :: Initial setup mismatch");
    return -1;
  }

  for (Uint32 test=0; test < totalTests; test++)
  {
    Uint32 len= rand() % maxBitsToCopy;
    Uint32 slack= maxBitsToCopy - len;
    Uint32 srcPos= slack ? rand() % slack : 0;
    Uint32 dstPos= slack ? rand() % slack : 0;

    if (BITMASK_DEBUG)
      ndbout_c("copyField :: Running test with len=%u, srcPos=%u, dstPos=%u, "
               "srcOff=%u, dstOff=%u",
               len, srcPos, dstPos, srcPos & 31, dstPos & 31);

    /* Run the copy */
    BitmaskImpl::copyField(targetCopy, dstPos, sourceBuf, srcPos, len);

    /* Do the equivalent action */
    for (Uint32 i=0; i< len; i++)
      BitmaskImpl::set(numWords, targetTest, dstPos + i,
                       BitmaskImpl::get(numWords, sourceBuf, srcPos+i));

    bool fail= false;
    /* Compare results */
    for (Uint32 i=0; i<maxBitsToCopy; i++)
    {
      if (BitmaskImpl::get(numWords, targetCopy, i) !=
          BitmaskImpl::get(numWords, targetTest, i))
      {
        ndbout_c("copyField :: Mismatch at bit %u, should be %u but is %u",
                 i, 
                 BitmaskImpl::get(numWords, targetTest, i),
                 BitmaskImpl::get(numWords, targetCopy, i));
        fail=true;
      }
    }

    if (fail)
      return -1;
  }

  return 0;
}

static
int checkNoTramplingGetSetField(const Uint32 totalTests)
{
  const Uint32 numWords= 67;
  const Uint32 maxBitsToCopy= (numWords * 32);
  Uint32 sourceBuf[numWords];
  Uint32 targetBuf[numWords];

  ndbout << "Testing : Bitmask NoTrampling"
         << endl;

  memset(sourceBuf, 0x00, (numWords*4));

  for (Uint32 test=0; test<totalTests; test++)
  {
    /* Always copy at least 1 bit */
    Uint32 srcStart= rand() % (maxBitsToCopy -1);
    Uint32 length= (rand() % ((maxBitsToCopy -1) - srcStart)) + 1;

    if (BITMASK_DEBUG)
      ndbout << "Testing start %u, length %u \n"
             << srcStart
             << length;
    // Set target to all ones.
    memset(targetBuf, 0xff, (numWords*4));

    BitmaskImpl::getField(numWords, sourceBuf, srcStart, length, targetBuf);

    // Check that there is no trampling
    Uint32 firstUntrampledWord= (length + 31)/32;

    for (Uint32 word=0; word< numWords; word++)
    {
      Uint32 targetWord= targetBuf[word];
      if (BITMASK_DEBUG)
        ndbout << "word=%d, targetWord=%u, firstUntrampledWord..=%u"
               << word << targetWord << firstUntrampledWord;

      if (! (word < firstUntrampledWord) ?
          (targetWord == 0) :
          (targetWord == 0xffffffff))
      {
        ndbout << "Notrampling getField failed for srcStart "
               << srcStart
               << " length " << length
               << " at word " << word << "\n";
        ndbout << "word=%d, targetWord=%u, firstUntrampledWord..=%u"
               << word << targetWord << firstUntrampledWord;
        return -1;
      }

    }

    /* Set target back to all ones. */
    memset(targetBuf, 0xff, (numWords*4));

    BitmaskImpl::setField(numWords, targetBuf, srcStart, length, sourceBuf);

    /* Check we've got all ones, with zeros only where expected */
    for (Uint32 word=0; word< numWords; word++)
    {
      Uint32 targetWord= targetBuf[word];

      for (Uint32 bit=0; bit< 32; bit++)
      {
        Uint32 bitNum= (word << 5) + bit;
        bool expectedValue= !((bitNum >= srcStart) &&
                              (bitNum < (srcStart + length)));
        bool actualValue= (((targetWord >> bit) & 1) == 1);
        if (BITMASK_DEBUG)
          ndbout << "bitNum=%u expectedValue=%u, actual value=%u"
                 << bitNum << expectedValue << actualValue;

        if (actualValue != expectedValue)
        {
          ndbout << "Notrampling setField failed for srcStart "
                 << srcStart
                 << " length " << length
                 << " at word " << word << " bit " << bit <<  "\n";
          ndbout << "bitNum=%u expectedValue=%u, actual value=%u"
                 << bitNum << expectedValue << actualValue;
          return -1;
        }
      }
    }

  }

  return 0;
}

static
int simple(int pos, int size)
{
  ndbout << "Testing : Bitmask simple pos: " << pos << " size: " << size
         << endl;

  Vector<Uint32> _mask;
  Vector<Uint32> _src;
  Vector<Uint32> _dst;
  Uint32 sz32 = (size + pos + 32) >> 5;
  const Uint32 sz = 4 * sz32;

  Uint32 zero = 0;
  _mask.fill(sz32+1, zero);
  _src.fill(sz32+1, zero);
  _dst.fill(sz32+1, zero);

  Uint32 * src = _src.getBase();
  Uint32 * dst = _dst.getBase();
  Uint32 * mask = _mask.getBase();

  memset(src, 0x0, sz);
  memset(dst, 0x0, sz);
  memset(mask, 0xFF, sz);
  rand(src, size);
  BitmaskImpl::setField(sz32, mask, pos, size, src);
  BitmaskImpl::getField(sz32, mask, pos, size, dst);
  if (BITMASK_DEBUG)
  {
    printf("src: "); print(src, size+31); printf("\n");
    printf("msk: "); print(mask, (sz32 << 5) + 31); printf("\n");
    printf("dst: "); print(dst, size+31); printf("\n");
  }
  return (cmp(src, dst, size+31)?0 : -1);
};

struct Alloc
{
  Uint32 pos;
  Uint32 size;
  Vector<Uint32> data;
};

static
int
testRanges(Uint32 bitmask_size)
{
  Vector<Alloc> alloc_list;
  bitmask_size = (bitmask_size + 31) & ~31;
  Uint32 sz32 = (bitmask_size >> 5);
  Vector<Uint32> alloc_mask;
  Vector<Uint32> test_mask;

  ndbout_c("Testing : Bitmask ranges for bitmask of size %d", bitmask_size);
  Uint32 zero = 0;
  alloc_mask.fill(sz32, zero);
  test_mask.fill(sz32, zero);

  /* Loop a number of times, setting and clearing bits in the mask
   * and tracking the modifications in a separate structure.
   * Check that both structures remain in sync
   */
  for(int i = 0; i<5000; i++)
  {
    Vector<Uint32> tmp;
    tmp.fill(sz32, zero);

    Uint32 pos = lrand() % (bitmask_size - 1);
    Uint32 free = 0;
    if(BitmaskImpl::get(sz32, alloc_mask.getBase(), pos))
    {
      // Bit was allocated
      // 1) Look up allocation
      // 2) Check data
      // 3) free it
      unsigned j;
      Uint32 min, max;
      for(j = 0; j<alloc_list.size(); j++)
      {
	min = alloc_list[j].pos;
	max = min + alloc_list[j].size;
	if(pos >= min && pos < max)
	{
	  break;
	}
      }
      if (! ((pos >= min) && (pos < max)))
      {
        printf("Failed with pos %u, min %u, max %u\n",
               pos, min, max);
        return -1;
      }
      BitmaskImpl::getField(sz32, test_mask.getBase(), min, max-min,
			    tmp.getBase());
      if(BITMASK_DEBUG)
      {
	printf("freeing [ %d %d ]", min, max);
	printf("- mask: ");
	print(tmp.getBase(), max - min);

	printf(" save: ");
        unsigned k;
	Alloc& a = alloc_list[j];
	for(k = 0; k<a.data.size(); k++)
	  printf("%.8x ", a.data[k]);
	printf("\n");
      }
      if(!cmp(tmp.getBase(), alloc_list[j].data.getBase(), max - min))
      {
	return -1;
      }
      while(min < max)
	BitmaskImpl::clear(sz32, alloc_mask.getBase(), min++);
      alloc_list.erase(j);
    }
    else
    {
      Vector<Uint32> tmp;
      tmp.fill(sz32, zero);

      // Bit was free
      // 1) Check how much space is avaiable
      // 2) Create new allocation of lrandom size
      // 3) Fill data with lrandom data
      // 4) Update alloc mask
      while(pos+free < bitmask_size &&
	    !BitmaskImpl::get(sz32, alloc_mask.getBase(), pos+free))
	free++;

      Uint32 sz =
	(free <= 64 && ((lrand() % 100) > 80)) ? free : (lrand() % free);
      sz = sz ? sz : 1;
      sz = pos + sz == bitmask_size ? sz - 1 : sz;
      Alloc a;
      a.pos = pos;
      a.size = sz;
      a.data.fill(((sz+31)>> 5)-1, zero);
      if(BITMASK_DEBUG)
	printf("pos %d -> alloc [ %d %d ]", pos, pos, pos+sz);
      for(Uint32 j = 0; j<sz; j++)
      {
	BitmaskImpl::set(sz32, alloc_mask.getBase(), pos+j);
	if((lrand() % 1000) > 500)
	  BitmaskImpl::set((sz + 31) >> 5, a.data.getBase(), j);
      }
      if(BITMASK_DEBUG)
      {
	printf("- mask: ");
	print(a.data.getBase(), sz);
	printf("\n");
      }
      BitmaskImpl::setField(sz32, test_mask.getBase(), pos, sz,
			    a.data.getBase());
      alloc_list.push_back(a);
    }
  }

#define NDB_BM_SUPPORT_RANGE
#ifdef NDB_BM_SUPPORT_RANGE
  for(Uint32 i = 0; i<1000; i++)
  {
    Uint32 sz32 = 10+rand() % 100;
    Uint32 zero = 0;
    Vector<Uint32> map;
    map.fill(sz32, zero);

    Uint32 sz = 32 * sz32;
    Uint32 start = (rand() % sz);
    Uint32 stop = start + ((rand() % (sz - start)) & 0xFFFFFFFF);

    Vector<Uint32> check;
    check.fill(sz32, zero);

    /* Verify range setting method works correctly */
    for(Uint32 j = 0; j<sz; j++)
    {
      bool expect = (j >= start && j<= stop);
      if(expect)
	BitmaskImpl::set(sz32, check.getBase(), j);
    }

    BitmaskImpl::setRange(sz32, map.getBase(), start, stop - start + 1);
    if (!BitmaskImpl::equal(sz32, map.getBase(), check.getBase()))
    {
      ndbout_c(" FAIL 1 sz: %d [ %d %d ]", sz, start, stop);
      printf("check: ");
      for(Uint32 j = 0; j<sz32; j++)
	printf("%.8x ", check[j]);
      printf("\n");

      printf("map  : ");
      for(Uint32 j = 0; j<sz32; j++)
	printf("%.8x ", map[j]);
      printf("\n");
      return -1;
    }

    map.clear();
    check.clear();

    /* Verify range clearing method works correctly */
    Uint32 one = ~(Uint32)0;
    map.fill(sz32, one);
    check.fill(sz32, one);

    for(Uint32 j = 0; j<sz; j++)
    {
      bool expect = (j >= start && j<stop);
      if(expect)
	BitmaskImpl::clear(sz32, check.getBase(), j);
    }

  }
#endif

  return 0;
}

static
int
testBitmask()
{
  /* Some testcases from storage/ndb/src/common/util/Bitmask.cpp */
  int res= 0;

  if ((res= checkNoTramplingGetSetField(100 /* totalTests */)) != 0)
    return res;

  if ((res= checkCopyField(1000)) != 0)
    return res;

  if ((res= simple(rand() % 33, // position
                   (rand() % 63)+1) // size
       ) != 0)
    return res;

  if ((res= testRanges(1+(rand() % 1000) // bitmask size
                       )) != 0)
    return res;

  return 0;
}

template class Vector<Alloc>;
