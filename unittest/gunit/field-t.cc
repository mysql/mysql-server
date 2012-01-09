/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved. 

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"

#include "field.h"
#include "sql_time.h"

namespace {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class FieldTest : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    Server_initializer::SetUpTestCase();
  }

  static void TearDownTestCase()
  {
    Server_initializer::TearDownTestCase();
  }

  virtual void SetUp()
  {
    initializer.SetUp();
  }

  virtual void TearDown()
  {
    initializer.TearDown();
  }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};


static void compareMysqlTime(const MYSQL_TIME& first, const MYSQL_TIME& second) 
{
  EXPECT_EQ(first.year, second.year);
  EXPECT_EQ(first.month, second.month);
  EXPECT_EQ(first.day, second.day);
  EXPECT_EQ(first.hour, second.hour);
  EXPECT_EQ(first.minute, second.minute);
  EXPECT_EQ(first.second, second.second);
  EXPECT_EQ(first.second_part, second.second_part);
  EXPECT_EQ(first.neg, second.neg);
  EXPECT_EQ(first.time_type, second.time_type);
}

class Mock_table : public TABLE
{
public:
  Mock_table(THD *thd)
  {
    null_row= false;
    read_set= 0;
    in_use= thd;
  }
};

// A mock Protocol class to be able to test Field::send_binary
// It just verifies that store_time has been passed what is expected
class Mock_protocol : public Protocol
{
private:
  MYSQL_TIME t;
  uint p;
public:
  Mock_protocol(THD *thd) : Protocol(thd) {}

  virtual bool store_time(MYSQL_TIME *time, uint precision)
  {
    t= *time;
    p= precision;
    return false;
  }  

  void verify_time(MYSQL_TIME *time, uint precision)
  {
    compareMysqlTime(*time, t);
    EXPECT_EQ(precision, p);
  }

  // Lots of functions that require implementation
  virtual void prepare_for_resend() {}

  virtual bool store_null() { return false; }
  virtual bool store_tiny(longlong from) { return false; }
  virtual bool store_short(longlong from) { return false; }
  virtual bool store_long(longlong from) { return false; }
  virtual bool store_longlong(longlong from, bool unsigned_flag) { return false; }
  virtual bool store_decimal(const my_decimal *) { return false; }
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *cs) { return false; }
  virtual bool store(const char *from, size_t length, 
                     const CHARSET_INFO *fromcs,
                     const CHARSET_INFO *tocs) { return false; }
  virtual bool store(float from, uint32 decimals, String *buffer) { return false; }
  virtual bool store(double from, uint32 decimals, String *buffer) { return false; }
  virtual bool store(MYSQL_TIME *time, uint precision) { return false; }
  virtual bool store_date(MYSQL_TIME *time) { return false; }
  virtual bool store(Field *field) { return false; }

  virtual bool send_out_parameters(List<Item_param> *sp_params) { return false; }
  virtual enum enum_protocol_type type() { return PROTOCOL_LOCAL; };
};


TEST_F(FieldTest, FieldTimef)
{
  uchar fieldBuf[6];
  uchar nullPtr[1]= {0};
  MYSQL_TIME time= {0, 0, 0, 12, 23, 12, 123400, false, MYSQL_TIMESTAMP_TIME};

  Field_timef* field= new Field_timef(fieldBuf, nullPtr, false, Field::NONE, 
				      "f1", 4);
  // Test public member functions
  EXPECT_EQ(4UL, field->decimals()); //TS-TODO
  EXPECT_EQ(MYSQL_TYPE_TIME, field->type());
  EXPECT_EQ(MYSQL_TYPE_TIME2, field->binlog_type());

  longlong packed= TIME_to_longlong_packed(&time);

  EXPECT_EQ(0, field->store_packed(packed));
  EXPECT_DOUBLE_EQ(122312.1234, field->val_real());
  EXPECT_EQ(122312, field->val_int());
  EXPECT_EQ(packed, field->val_time_temporal());

  my_decimal decval;
  my_decimal* dec= field->val_decimal(&decval);
  double res;
  my_decimal2double(0, dec, &res);
  EXPECT_DOUBLE_EQ(122312.1234, res);

  EXPECT_EQ(5UL, field->pack_length());
  EXPECT_EQ(5UL, field->pack_length_from_metadata(4));
  EXPECT_EQ(5UL, field->row_pack_length());

  String str(7);
  field->sql_type(str);
  EXPECT_STREQ("time(4)", str.c_ptr_safe());

  EXPECT_EQ(1, field->zero_pack());
  EXPECT_EQ(&my_charset_bin, field->sort_charset());

  // Test clone
  Field* copy= field->clone();
  EXPECT_EQ(field->decimals(), copy->decimals());
  EXPECT_EQ(field->type(), copy->type());
  EXPECT_DOUBLE_EQ(field->val_real(), copy->val_real());
  EXPECT_EQ(field->val_int(), copy->val_int());
  EXPECT_EQ(field->val_time_temporal(), copy->val_time_temporal());
  EXPECT_EQ(0, field->cmp(field->ptr, copy->ptr));

  // Test reset
  EXPECT_EQ(0, field->reset());
  EXPECT_DOUBLE_EQ(0.0, field->val_real());
  EXPECT_EQ(0, field->val_int());
  
  // Test inherited member functions
  // Functions inherited from Field_time_common
  field->store_time(&time, 4);
  EXPECT_EQ(4UL, field->decimals());
  EXPECT_EQ(MYSQL_TYPE_TIME, field->type());
  EXPECT_DOUBLE_EQ(122312.1234, field->val_real());
  EXPECT_EQ(122312, field->val_int());
  EXPECT_EQ(packed, field->val_time_temporal());

  String timeStr(15);
  EXPECT_STREQ("12:23:12.1234", field->val_str(&timeStr, &timeStr)->c_ptr());

  field->store_time(&time, 0);
  EXPECT_DOUBLE_EQ(122312.1234, field->val_real());  // Correct?

  MYSQL_TIME dateTime;
  MYSQL_TIME bigTime= {0, 0, 0, 123, 45, 45, 555500, false, MYSQL_TIMESTAMP_TIME};
  EXPECT_EQ(0, field->store_time(&bigTime, 4));
  EXPECT_FALSE(field->get_date(&dateTime, 0));

  make_datetime((DATE_TIME_FORMAT *)0, &dateTime, &timeStr, 6);
  EXPECT_STREQ("1970-01-06 03:45:45.555500", timeStr.c_ptr());

  MYSQL_TIME t;
  EXPECT_FALSE(field->get_time(&t));
  compareMysqlTime(bigTime, t);

  Mock_protocol protocol(thd());
  EXPECT_FALSE(field->send_binary(&protocol));
  // The verification below fails because send_binary move hours to days 
  // protocol.verify_time(&bigTime, 0);  // Why 0?

  // Function inherited from Field_temporal
  EXPECT_TRUE(field->is_temporal());
  EXPECT_EQ(STRING_RESULT, field->result_type());
  EXPECT_EQ(15UL, field->max_display_length());
  EXPECT_TRUE(field->str_needs_quotes());
  
  // Not testing is_equal() yet, will require a mock TABLE object
  //  Create_field cf(field, field);
  //  EXPECT_TRUE(field->is_equal(&cf));
  
  EXPECT_EQ(DECIMAL_RESULT, field->numeric_context_result_type());
  EXPECT_EQ(INT_RESULT, field->cmp_type());  
  EXPECT_EQ(INT_RESULT, field->cmp_type());  
  EXPECT_EQ(DERIVATION_NUMERIC, field->derivation());
  EXPECT_EQ(&my_charset_numeric, field->charset());
  EXPECT_TRUE(field->can_be_compared_as_longlong());
  EXPECT_TRUE(field->binary());
  // Below is not tested, because of ASSERT
  // EXPECT_EQ(TIMESTAMP_NO_AUTO_SET, field->get_auto_set_type());
  
  // Not testing make_field, it also needs a mock TABLE object
  
  EXPECT_EQ(0, field->store("12:23:12.123456", 15, &my_charset_numeric));
  EXPECT_DOUBLE_EQ(122312.1235, field->val_real());

  EXPECT_EQ(0, field->store_decimal(dec));
  EXPECT_DOUBLE_EQ(122312.1234, field->val_real());

  EXPECT_EQ(0, field->store(-234545, false));
  EXPECT_DOUBLE_EQ(-234545.0, field->val_real());
  
  {
    // Test that store() with a to big number gives right error
    Mock_error_handler error_handler(thd(), ER_TRUNCATED_WRONG_VALUE);
    EXPECT_EQ(1, field->store(0x80000000, true));
    // Test that error handler was actually called
    EXPECT_EQ(1, error_handler.handle_called());
    // Test that field contains expecte max time value
    EXPECT_DOUBLE_EQ(8385959, field->val_real());  // Max time value
  }

  EXPECT_EQ(0, field->store(1234545.555555));
  EXPECT_DOUBLE_EQ(1234545.5556, field->val_real());

  // Some of the functions inherited from Field
  Field *f= field;
  EXPECT_EQ(0, f->store_time(&time, MYSQL_TIMESTAMP_TIME)); 
  EXPECT_DOUBLE_EQ(122312.1234, f->val_real());  // Why decimals  here?
  EXPECT_STREQ("12:23:12.1234", f->val_str(&timeStr)->c_ptr());
  EXPECT_STREQ("122312", f->val_int_as_str(&timeStr, false)->c_ptr());
  EXPECT_TRUE(f->eq(copy));
  EXPECT_TRUE(f->eq_def(copy));
  
  // Not testing store(const char, uint, const CHARSET_INFO *, enum_check_fields)
  // it requires a mock table
  
  Mock_table m_table(thd());
  f->table= &m_table;
  struct timeval tv;
  int warnings= 0;
  EXPECT_EQ(0, f->get_timestamp(&tv, &warnings));
  // EXPECT_EQ(40992, tv.tv_sec);  // This is 11:23:12.  Why?  Time zone?
  EXPECT_EQ(123400, tv.tv_usec);

  delete field;

}

TEST_F(FieldTest, FieldTimefCompare)
{
  const int nFields= 7;
  uchar fieldBufs[nFields][6];
  uchar nullPtrs[nFields];

  MYSQL_TIME times[nFields]= {
    {0, 0, 0, 12, 23, 12, 100000, true,  MYSQL_TIMESTAMP_TIME},
    {0, 0, 0,  0,  0,  0,  10000, true,  MYSQL_TIMESTAMP_TIME},
    {0, 0, 0,  0,  0,  0,      0, false, MYSQL_TIMESTAMP_TIME},
    {0, 0, 0,  0,  0,  0, 999900, false, MYSQL_TIMESTAMP_TIME},
    {0, 0, 0,  0,  0,  0, 999990, false, MYSQL_TIMESTAMP_TIME},
    {0, 0, 0, 11, 59, 59, 999999, false, MYSQL_TIMESTAMP_TIME},
    {0, 0, 0, 12, 00, 00, 100000, false, MYSQL_TIMESTAMP_TIME}};
    
  Field* fields[nFields];
  uchar sortStrings[nFields][6];
  for (int i=0; i < nFields; ++i)
  {
    char fieldName[3];
    sprintf(fieldName, "f%c", i);
    fields[i]= new Field_timef(fieldBufs[i], nullPtrs+i, false, Field::NONE, 
			       fieldName, 6);

    longlong packed= TIME_to_longlong_packed(&times[i]);
    EXPECT_EQ(0, fields[i]->store_packed(packed));
    fields[i]->sort_string(sortStrings[i], fields[i]->pack_length());
  }

  for (int i=0; i < nFields; ++i)
    for (int j=0; j < nFields; ++j)
    {
      String tmp;
      if (i < j)
      {
	EXPECT_GT(0, memcmp(sortStrings[i], sortStrings[j], 
			    fields[i]->pack_length()))
	  << fields[i]->val_str(&tmp)->c_ptr() << " < " 
	  << fields[j]->val_str(&tmp)->c_ptr(); 
	EXPECT_GT(0, fields[i]->cmp(fields[i]->ptr, fields[j]->ptr))
	  << fields[i]->val_str(&tmp)->c_ptr() << " < " 
	  << fields[j]->val_str(&tmp)->c_ptr();
      }
      else if (i > j)
      {
	EXPECT_LT(0, memcmp(sortStrings[i], sortStrings[j],
			    fields[i]->pack_length()))
	  << fields[i]->val_str(&tmp)->c_ptr() << " > " 
	  << fields[j]->val_str(&tmp)->c_ptr();
	EXPECT_LT(0, fields[i]->cmp(fields[i]->ptr, fields[j]->ptr))
	  << fields[i]->val_str(&tmp)->c_ptr() << " > " 
	  << fields[j]->val_str(&tmp)->c_ptr();
      }
      else
      {
	EXPECT_EQ(0, memcmp(sortStrings[i], sortStrings[j],
			    fields[i]->pack_length()))
	  << fields[i]->val_str(&tmp)->c_ptr() << " = " 
	  << fields[j]->val_str(&tmp)->c_ptr();
	EXPECT_EQ(0, fields[i]->cmp(fields[i]->ptr, fields[j]->ptr))
	  << fields[i]->val_str(&tmp)->c_ptr() << " = "
	  << fields[j]->val_str(&tmp)->c_ptr();
      }
    }
}


TEST_F(FieldTest, FieldTime)
{
  uchar fieldBuf[6];
  uchar nullPtr[1]= {0};
  MYSQL_TIME bigTime= {0, 0, 0, 123, 45, 45, 555500, false, MYSQL_TIMESTAMP_TIME};

  Field_time* field= new Field_time(fieldBuf, nullPtr, false, Field::NONE,
				     "f1");
  EXPECT_EQ(0, field->store_time(&bigTime, 4));
  MYSQL_TIME t;
  EXPECT_FALSE(field->get_time(&t));
  compareMysqlTime(bigTime, t);
}


}
