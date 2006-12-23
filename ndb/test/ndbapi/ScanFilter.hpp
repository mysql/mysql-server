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

#ifndef SCAN_FILTER_HPP
#define SCAN_FILTER_HPP

class ScanFilter {
public:
#if 0
  /**
   * Create a scan filter for table tab
   * colNo - column to filter on
   * val - val to use when selecting valu to filter on
   *
   */
  ScanFilter(const NDBT_Table& tab, 
	     int colNo,
	     int val);
#endif
  ScanFilter(int records = 1000){};
  virtual int filterOp(NdbOperation*) = 0;
  virtual int verifyRecord(NDBT_ResultRow&) = 0;
private:

  //  const NDBT_Table& tab;
};

class LessThanFilter : public ScanFilter {
public:
  LessThanFilter(int records){ compare_value = records / 100; };
private:
  Uint32 compare_value;
  int filterOp(NdbOperation* pOp);
  int verifyRecord(NDBT_ResultRow&);
};

class EqualFilter : public ScanFilter {
  static const Uint32 compare_value = 100;
  int filterOp(NdbOperation* pOp);
  int verifyRecord(NDBT_ResultRow&);
};

class NoFilter : public ScanFilter {
  int filterOp(NdbOperation* pOp);
  int verifyRecord(NDBT_ResultRow&);
};


int LessThanFilter::filterOp(NdbOperation* pOp){
  
  if (pOp->load_const_u32(1, compare_value) != 0)
    return NDBT_FAILED;

  if (pOp->read_attr("KOL2", 2) != 0)
    return NDBT_FAILED;

  if (pOp->branch_lt(1, 2, 0) != 0)
    return NDBT_FAILED;

  if (pOp->interpret_exit_nok() != 0)
    return NDBT_FAILED;
  
  if (pOp->def_label(0) != 0)
    return NDBT_FAILED;

  if (pOp->interpret_exit_ok() != 0)
    return NDBT_FAILED;

  return NDBT_OK;
}

int LessThanFilter::verifyRecord(NDBT_ResultRow& row){
  NdbRecAttr* rec = row.attributeStore(1);
  if (rec->u_32_value() < compare_value)
    return NDBT_OK;
  return NDBT_FAILED;
}

int EqualFilter::filterOp(NdbOperation* pOp){
  
  if (pOp->load_const_u32(1, compare_value) != 0)
    return NDBT_FAILED;

  if (pOp->read_attr("KOL2", 2) != 0)
    return NDBT_FAILED;

  if (pOp->branch_eq(1, 2, 0) != 0)
    return NDBT_FAILED;

  if (pOp->interpret_exit_nok() != 0)
    return NDBT_FAILED;
  
  if (pOp->def_label(0) != 0)
    return NDBT_FAILED;

  if (pOp->interpret_exit_ok() != 0)
    return NDBT_FAILED;

  return NDBT_OK;
}

int EqualFilter::verifyRecord(NDBT_ResultRow& row){
  NdbRecAttr* rec = row.attributeStore(1);
  if (rec->u_32_value() == compare_value)
    return NDBT_OK;
  return NDBT_FAILED;
}

int NoFilter::filterOp(NdbOperation* pOp){
  return NDBT_OK;
}

int NoFilter::verifyRecord(NDBT_ResultRow& row){
  // Check if this record should be in the result set or not
  return NDBT_OK;
}

#endif
