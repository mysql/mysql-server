/* Copyright (C) 2007 MySQL AB

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>

#define ERR_EXIT(obj, msg) \
do \
{ \
fprintf(stderr, "%s: %s (%d) in %s:%d\n", \
msg, obj->getNdbError().message, obj->getNdbError().code, __FILE__, __LINE__); \
exit(-1); \
} \
while (0);

#define PRINT_ERROR(code,msg) \
do \
{ \
fprintf(stderr, "Error in %s, line: %d, code: %d, msg: %s.\n", __FILE__, __LINE__, code, msg); \
} \
while (0);

#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(-1); }
#define APIERROR(error) { \
  PRINT_ERROR(error.code,error.message); \
  exit(-1); }

#define TEST_NAME "TestScanFilter" 
#define TABLE_NAME "TABLE_SCAN"

const char *COL_NAME[] = {"id", "i", "j", "k", "l", "m", "n"}; 
const char COL_LEN = 7;
/*
* Not to change TUPLE_NUM, because the column in TABLE_NAME is fixed,
* there are six columns, 'i', 'j', 'k', 'l', 'm', 'n', and each on is equal to 1 or 1,
* Since each tuple should be unique in this case, then TUPLE_NUM = 2 power 6 = 64 
*/
#ifdef _AIX
/*
  IBM xlC_r breaks on the initialization with pow():
  "The expression must be an integral constant expression."
*/
const int TUPLE_NUM = 64;
#else
const int TUPLE_NUM = (int)pow(2, COL_LEN-1);
#endif

/*
* the recursive level of random scan filter, can 
* modify this parameter more or less, range from 
* 1 to 100, larger num consumes more scan time
*/
const int RECURSIVE_LEVEL = 10;

const int MAX_STR_LEN = (RECURSIVE_LEVEL * (COL_LEN+1) * 4);

/*
* Each time stands for one test, it will produce a random 
* filter string, and scan through ndb api and through 
* calculation with tuples' data, then compare the result, 
* if they are equal, this test passed, or failed.
* Only all TEST_NUM times tests passed, we can believe 
* the suite of test cases are okay.
* Change TEST_NUM to larger will need more time to test
*/
const int TEST_NUM = 5000;


/* Table definition*/
static
const
NDBT_Attribute MYTAB1Attribs[] = {
  NDBT_Attribute("id", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("i", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("j", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("k", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("l", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("m", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("n", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table MYTAB1(TABLE_NAME, sizeof(MYTAB1Attribs)/sizeof(NDBT_Attribute), MYTAB1Attribs);


int createTable(Ndb* pNdb, const NdbDictionary::Table* tab, bool _temp, 
			 bool existsOk, NDBT_CreateTableHook f)
{
  int r = 0;
  do{
    NdbDictionary::Table tmpTab(* tab);
    tmpTab.setStoredTable(_temp ? 0 : 1);
    if(f != 0 && f(pNdb, tmpTab, 0, NULL))
    {
      ndbout << "Failed to create table" << endl;
      return NDBT_FAILED;
    }      
    r = pNdb->getDictionary()->createTable(tmpTab);
    if(r == -1){
      if(!existsOk){
	ndbout << "Error: " << pNdb->getDictionary()->getNdbError() << endl;
	break;
      }
      if(pNdb->getDictionary()->getNdbError().code != 721){
	ndbout << "Error: " << pNdb->getDictionary()->getNdbError() << endl;
	break;
      }
      r = 0;
    }
  }while(false);
  
  return r;
}

/*
* Function to produce the tuples' data
*/
int runPopulate(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb *myNdb = GETNDB(step);
  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable(TABLE_NAME);
  if(myTable == NULL) 
    APIERROR(myDict->getNdbError());

  NdbTransaction* myTrans = myNdb->startTransaction();
  if (myTrans == NULL)
    APIERROR(myNdb->getNdbError());

  for(int num = 0; num < TUPLE_NUM; num++) 
  {
    NdbOperation* myNdbOperation = myTrans->getNdbOperation(myTable);
    if(myNdbOperation == NULL) 
    {
      APIERROR(myTrans->getNdbError());
    }

/* the tuples' data in TABLE_NAME
+----+---+---+---+---+---+---+
| id | i | j | k | l | m | n |
+----+---+---+---+---+---+---+
|  0 | 0 | 0 | 0 | 0 | 0 | 0 |
|  1 | 0 | 0 | 0 | 0 | 0 | 1 |
|  2 | 0 | 0 | 0 | 0 | 1 | 0 |
|  3 | 0 | 0 | 0 | 0 | 1 | 1 |
|  4 | 0 | 0 | 0 | 1 | 0 | 0 |
|  5 | 0 | 0 | 0 | 1 | 0 | 1 |
|  6 | 0 | 0 | 0 | 1 | 1 | 0 |
|  7 | 0 | 0 | 0 | 1 | 1 | 1 |
|  8 | 0 | 0 | 1 | 0 | 0 | 0 |
|  9 | 0 | 0 | 1 | 0 | 0 | 1 |
| 10 | 0 | 0 | 1 | 0 | 1 | 0 |
| 11 | 0 | 0 | 1 | 0 | 1 | 1 |
| 12 | 0 | 0 | 1 | 1 | 0 | 0 |
| 13 | 0 | 0 | 1 | 1 | 0 | 1 |
| 14 | 0 | 0 | 1 | 1 | 1 | 0 |
| 15 | 0 | 0 | 1 | 1 | 1 | 1 |
| 16 | 0 | 1 | 0 | 0 | 0 | 0 |
| 17 | 0 | 1 | 0 | 0 | 0 | 1 |
| 18 | 0 | 1 | 0 | 0 | 1 | 0 |
| 19 | 0 | 1 | 0 | 0 | 1 | 1 |
| 20 | 0 | 1 | 0 | 1 | 0 | 0 |
| 21 | 0 | 1 | 0 | 1 | 0 | 1 |
| 22 | 0 | 1 | 0 | 1 | 1 | 0 |
| 23 | 0 | 1 | 0 | 1 | 1 | 1 |
| 24 | 0 | 1 | 1 | 0 | 0 | 0 |
| 25 | 0 | 1 | 1 | 0 | 0 | 1 |
| 26 | 0 | 1 | 1 | 0 | 1 | 0 |
| 27 | 0 | 1 | 1 | 0 | 1 | 1 |
| 28 | 0 | 1 | 1 | 1 | 0 | 0 |
| 29 | 0 | 1 | 1 | 1 | 0 | 1 |
| 30 | 0 | 1 | 1 | 1 | 1 | 0 |
| 31 | 0 | 1 | 1 | 1 | 1 | 1 |
| 32 | 1 | 0 | 0 | 0 | 0 | 0 |
| 33 | 1 | 0 | 0 | 0 | 0 | 1 |
| 34 | 1 | 0 | 0 | 0 | 1 | 0 |
| 35 | 1 | 0 | 0 | 0 | 1 | 1 |
| 36 | 1 | 0 | 0 | 1 | 0 | 0 |
| 37 | 1 | 0 | 0 | 1 | 0 | 1 |
| 38 | 1 | 0 | 0 | 1 | 1 | 0 |
| 39 | 1 | 0 | 0 | 1 | 1 | 1 |
| 40 | 1 | 0 | 1 | 0 | 0 | 0 |
| 41 | 1 | 0 | 1 | 0 | 0 | 1 |
| 42 | 1 | 0 | 1 | 0 | 1 | 0 |
| 43 | 1 | 0 | 1 | 0 | 1 | 1 |
| 44 | 1 | 0 | 1 | 1 | 0 | 0 |
| 45 | 1 | 0 | 1 | 1 | 0 | 1 |
| 46 | 1 | 0 | 1 | 1 | 1 | 0 |
| 47 | 1 | 0 | 1 | 1 | 1 | 1 |
| 48 | 1 | 1 | 0 | 0 | 0 | 0 |
| 49 | 1 | 1 | 0 | 0 | 0 | 1 |
| 50 | 1 | 1 | 0 | 0 | 1 | 0 |
| 51 | 1 | 1 | 0 | 0 | 1 | 1 |
| 52 | 1 | 1 | 0 | 1 | 0 | 0 |
| 53 | 1 | 1 | 0 | 1 | 0 | 1 |
| 54 | 1 | 1 | 0 | 1 | 1 | 0 |
| 55 | 1 | 1 | 0 | 1 | 1 | 1 |
| 56 | 1 | 1 | 1 | 0 | 0 | 0 |
| 57 | 1 | 1 | 1 | 0 | 0 | 1 |
| 58 | 1 | 1 | 1 | 0 | 1 | 0 |
| 59 | 1 | 1 | 1 | 0 | 1 | 1 |
| 60 | 1 | 1 | 1 | 1 | 0 | 0 |
| 61 | 1 | 1 | 1 | 1 | 0 | 1 |
| 62 | 1 | 1 | 1 | 1 | 1 | 0 |
| 63 | 1 | 1 | 1 | 1 | 1 | 1 |
+----+---+---+---+---+---+---+
*/
    myNdbOperation->insertTuple();
    myNdbOperation->equal(COL_NAME[0], num);
    for(int col = 1; col < COL_LEN; col++)
    {
      myNdbOperation->setValue(COL_NAME[col], (num>>(COL_LEN-1-col))&1);
    }
  }

  int check = myTrans->execute(NdbTransaction::Commit);

  myTrans->close();

  if (check == -1)
    return NDBT_FAILED;
  else
    return NDBT_OK;

}



/*
* a=AND, o=OR, A=NAND, O=NOR
*/
char op_string[] = "aoAO";
/*
* the six columns' name of test table
*/
char col_string[] = "ijklmn";
const int op_len = strlen(op_string);
const int col_len = strlen(col_string);

/*
* get a random op from "aoAO"
*/
int get_rand_op_ch(char *ch)
{
  static unsigned int num = 0;
  if(++num == 0) 
    num = 1;
  srand(num*time(NULL));
  *ch = op_string[rand() % op_len];
  return 1;
}

/*
* get a random order form of "ijklmn" trough exchanging letter
*/
void change_col_order()
{
  int pos1,pos2;
  char temp;
  for (int i = 0; i < 10; i++)  //exchange for 10 times
  {
    srand(time(NULL)/(i+1));
    pos1 = rand() % col_len;
    srand((i+1)*time(NULL));
    pos2 = rand() % col_len;
    if (pos1 == pos2)
      continue;
    temp = col_string[pos1];
    col_string[pos1] = col_string[pos2];
    col_string[pos2] = temp; 
  }
}

/*
* get a random sub string of "ijklmn" 
*/
int get_rand_col_str(char *str)
{
  int len;
  static unsigned int num = 0;
  if(++num == 0) 
    num = 1;
  srand(num*time(NULL));
  len = rand() % col_len + 1;
  change_col_order();
  snprintf(str, len+1, "%s", col_string);  //len+1, including '\0'
  return len;
}

/*
* get a random string including operation and column 
* eg, Alnikx
*/
int get_rand_op_str(char *str)
{
  char temp[256];
  int len1, len2, len;
  len1 = get_rand_op_ch(temp);
  len2 = get_rand_col_str(temp+len1);
  len = len1 + len2;
  temp[len] = 'x';
  snprintf(str, len+1+1, "%s", temp);  //len+1, including '\0'
  return len+1;
}

/*
* replace a letter of source string with a new string 
* e.g., source string: 'Aijkx', replace i with new string 'olmx'
* then source string is changed to 'Aolmxjkx'
* source: its format should be produced from get_rand_op_str() 
* pos: range from 1 to strlen(source)-2
*/
int replace_a_to_str(char *source, int pos, char *newstr)
{
  char temp[MAX_STR_LEN];
  snprintf(temp, pos+1, "%s", source);
  snprintf(temp+pos, strlen(newstr)+1, "%s", newstr);
  snprintf(temp+pos+strlen(newstr), strlen(source)-pos, "%s", source+pos+1);
  snprintf(source, strlen(temp)+1, "%s", temp); 
  return strlen(source);
}

/*
* check whether the inputed char is an operation 
*/
bool check_op(char ch)
{
  if( ch == 'a' || ch == 'A' || ch == 'o' || ch == 'O')
    return true;
  else
    return false;
}

/*
* check whether the inputed char is end flag 
*/
bool check_end(char ch)
{
  return (ch == 'x');
}

/*
* check whether the inputed char is end flag 
*/
bool check_col(char ch)
{
  if( ch == 'i' || ch == 'j' || ch == 'k' 
    || ch == 'l' || ch == 'm' || ch == 'n' )
    return true;
  else
    return false;
}

/*
* To ensure we can get a random string with RECURSIVE_LEVEL,
* we need a position where can replace a letter with a new string. 
*/
int get_rand_replace_pos(char *str, int len)
{
  int pos_op = 0;
  int pos_x = 0;
  int pos_col = 0;
  int span = 0;
  static int num = 0;
  char temp;

  for(int i = 0; i < len; i++)
  {
    temp = str[i];
    if(! check_end(temp))
    {
      if(check_op(temp))  
        pos_op = i;
    }
    else
    {
      pos_x = i;
      break;
    }
  }

  if(++num == 0) 
    num = 1;

  span = pos_x - pos_op - 1;
  if(span <= 1)
  {
    pos_col = pos_op + 1;
  }
  else
  {
    srand(num*time(NULL));
    pos_col = pos_op + rand() % span + 1;
  }
  return pos_col;
}

/*
* Check whether the given random string is valid
* and applicable for this test case
*/
bool check_random_str(char *str)
{
  char *p;
  int op_num = 0;
  int end_num = 0;

  for(p = str; *p; p++)
  {
    bool tmp1 = false, tmp2 = false;
    if(tmp1 = check_op(*p))
      op_num++;
    if(tmp2 = check_end(*p))
      end_num++;
    if(!(tmp1 || tmp2 || check_col(*p)))   //there are illegal letters
      return false;
  }

  if(op_num != end_num)     //begins are not equal to ends
    return false;

  return true;
}

/*
* Get a random string with RECURSIVE_LEVEL 
*/
void get_rand_op_str_compound(char *str)
{
  char small_str[256];
  int pos;
  int tmp;
  int level;
  static int num = 0;

  if(++num == 0)
    num = 1;

  srand(num*time(NULL));
  level = 1 + rand() % RECURSIVE_LEVEL;
 
  get_rand_op_str(str);

  for(int i = 0; i < level; i++)
  {
    get_rand_op_str(small_str);
    tmp = strlen(small_str);
    get_rand_op_str(small_str + tmp);   //get two operations
    pos = get_rand_replace_pos(str, strlen(str));
    replace_a_to_str(str, pos, small_str);
  }

  //check the random string
  if(!check_random_str(str))
  {
    fprintf(stderr, "Error random string! \n");
    exit(-1);
  }
}

/*
* get column id of i,j,k,l,m,n
*/
int get_column_id(char ch)
{
  return (ch - 'i' + 1);  //from 1 to 6
}

/*
* check whether column value of the NO. tuple is equal to 1
* col_id: column id, range from 1 to 6
* tuple_no: record NO., range from 0 to 63 
*/
bool check_col_equal_one(int tuple_no, int col_id)
{
  int i = (int)pow((double)2, (double)(6 - col_id));
  int j = tuple_no / i;
  if(j % 2)
    return true;
  else
    return false;
}

/*
* get a result after all elements in the array with AND
* value: pointer to a bool array
* len: length of the bool array
*/
bool AND_op(bool *value, int len)
{
  for(int i = 0; i < len; i++)
  {
    if(! value[i])
      return false;
  }
  return true;
}

/*
* get a result after all elements in the array with OR
* value: pointer to a bool array
* len: length of the bool array
*/
bool OR_op(bool *value, int len)
{
  for(int i = 0; i < len; i++)
  {
    if(value[i])
      return true;
  }
  return false;
}

/*
* get a result after all elements in the array with NAND
* value: pointer to a bool array
* len: length of the bool array
*/
bool NAND_op(bool *value, int len)
{
  return (! AND_op(value, len));
}

/*
* get a result after all elements in the array with NOR
* value: pointer to a bool array
* len: length of the bool array
*/
bool NOR_op(bool *value, int len)
{
  return (! OR_op(value, len));
}

/*
* AND/NAND/OR/NOR operation for a bool array 
*/
bool calculate_one_op(char op_type, bool *value, int len)
{
  switch(op_type)
  {
    case 'a':
      return AND_op(value, len);
      break;
    case 'o':
      return OR_op(value, len);
      break;
    case 'A':
      return NAND_op(value, len);
      break;
    case 'O':
      return NOR_op(value, len);
      break;
  }
  return false;   //make gcc happy
}

typedef struct _stack_element
{
  char type;
  int num;
}stack_element;

/*
* stack_op, store info for AND,OR,NAND,NOR
* stack_col, store value of column(i,j,k,l,m,n) and temporary result for an operation
*/
stack_element stack_op[RECURSIVE_LEVEL * COL_LEN];
bool stack_col[RECURSIVE_LEVEL * COL_LEN * 2];

/*
* check whether the given tuple is chosen by judgement condition 
* tuple_no, the NO of tuple in TABLE_NAME, range from 0 to TUPLE_NUM
* str: a random string of scan opearation and condition
* len: length of str
*/
bool check_one_tuple(int tuple_no, char *str, int len)
{
  int pop_op = 0;
  int pop_col = 0;
  for(int i = 0; i < len; i++)
  {
    char letter = *(str + i);
    if(check_op(letter))    //push
    {
      stack_op[pop_op].type = letter;
      stack_op[pop_op].num = 0;
      pop_op++;
    }
    if(check_col(letter))   //push
    {
      stack_col[pop_col] = check_col_equal_one(tuple_no, get_column_id(letter));  
      pop_col++;
      stack_op[pop_op-1].num += 1;
    }
    if(check_end(letter))
    {
      if(pop_op <= 1)
      {
        return calculate_one_op(stack_op[pop_op-1].type, 
                                stack_col, 
                                stack_op[pop_op-1].num);
      }
      else
      {
        bool tmp1 = calculate_one_op(stack_op[pop_op-1].type, 
                                    stack_col + pop_col - stack_op[pop_op-1].num, 
                                    stack_op[pop_op-1].num);
        pop_col -= stack_op[pop_op-1].num;    //pop
        pop_op--;
        stack_col[pop_col] = tmp1;    //push
        pop_col++;
        stack_op[pop_op-1].num += 1;
      }
    }
  }
  return false;   //make gcc happy
}

/*
* get lists of tuples which match the scan condiction through calculating
* str: a random string of scan opearation and condition
*/
void check_all_tuples(char *str, bool *res)
{    
  for (int i = 0; i < TUPLE_NUM; i++)
  {
    if(check_one_tuple(i, str, strlen(str)))
      res[i] = true;
  }
}

/*
* convert a letter to group number what ndbapi need
*/
NdbScanFilter::Group get_api_group(char op_name)
{
  switch (op_name) {
  case 'a': return NdbScanFilter::AND;
  case 'o': return NdbScanFilter::OR;
  case 'A': return NdbScanFilter::NAND;
  case 'O': return NdbScanFilter::NOR;
  default: 
    fprintf(stderr, "Invalid group name %c !\n", op_name);
    exit(3);
  }
}

/*
* with ndbapi, call begin, eq/ne/lt/gt/le/ge..., end
*/
NdbScanFilter * call_ndbapi(char *str, NdbTransaction *transaction, 
  NdbScanOperation *scan, NdbDictionary::Column const *col[])
{
  NdbScanFilter *scanfilter = new NdbScanFilter(scan);
  char *p;

  for (p = str; *p; p++) 
  {
    if(check_op(*p))
    {
       if(scanfilter->begin(get_api_group(*p))) 
          ERR_EXIT(transaction, "filter begin() failed");
    }
    if(check_col(*p))
    {
       if(scanfilter->eq(col[*p-'i'+1]->getColumnNo(), (Uint32)1)) 
          ERR_EXIT(transaction, "filter eq() failed"); 
    }
    if(check_end(*p))
    {
      if(scanfilter->end()) 
        ERR_EXIT(transaction, "filter end() failed");
    }
  }
  
  return scanfilter;
}

/*
* get the tuples through ndbapi, and save the tuples NO.
* str: a random string of scan opearation and condition
*/
void ndbapi_tuples(Ndb *ndb, char *str, bool *res)
{
	const NdbDictionary::Dictionary *dict  = ndb->getDictionary();
	if (!dict) 
    ERR_EXIT(ndb, "Can't get dict");

	const NdbDictionary::Table *table = dict->getTable(TABLE_NAME);
	if (!table) 
    ERR_EXIT(dict, "Can't get table"TABLE_NAME);
	
	const NdbDictionary::Column *col[COL_LEN];
  for(int i = 0; i < COL_LEN; i++)
  {
    char tmp[128];
    col[i] = table->getColumn(COL_NAME[i]);
	  if(!col[i]) 
    {
      snprintf(tmp, 128, "Can't get column %s", COL_NAME[i]);
      ERR_EXIT(dict, tmp);
    }
  }

  NdbTransaction *transaction;
  NdbScanOperation *scan;
  NdbScanFilter *filter;

  transaction = ndb->startTransaction();
  if (!transaction) 
    ERR_EXIT(ndb, "Can't start transaction");

  scan = transaction->getNdbScanOperation(table);	
  if (!scan) 
    ERR_EXIT(transaction, "Can't get scan op");
	  
  if (scan->readTuples(NdbOperation::LM_Exclusive)) 
    ERR_EXIT(scan, "Can't set up read");
  
  NdbRecAttr *rec[COL_LEN];
  for(int i = 0; i < COL_LEN; i++)
  {
    char tmp[128];
    rec[i] = scan->getValue(COL_NAME[i]);
	  if(!rec[i]) 
    {
      snprintf(tmp, 128, "Can't get rec of %s", COL_NAME[i]);
      ERR_EXIT(scan, tmp);
    }
  }

  filter = call_ndbapi(str, transaction, scan, col);
	  
  if (transaction->execute(NdbTransaction::NoCommit)) 
    ERR_EXIT(transaction, "Can't execute");
	  
  int i,j,k,l,m,n;
  while (scan->nextResult(true) == 0) 
  {
    do 
    {
      i = rec[1]->u_32_value();
      j = rec[2]->u_32_value();
      k = rec[3]->u_32_value();
      l = rec[4]->u_32_value();
      m = rec[5]->u_32_value();
      n = rec[6]->u_32_value();
      res[32*i+16*j+8*k+4*l+2*m+n] = true;
    } while (scan->nextResult(false) == 0);
  }
	  
  delete filter;
  transaction->close();
}

/*
* compare the result between calculation and NDBAPI
* str: a random string of scan opearation and condition
* return: true stands for ndbapi ok, false stands for ndbapi failed
*/
template class Vector<bool>;
bool compare_cal_ndb(char *str, Ndb *ndb)
{
  Vector<bool> res_cal;
  Vector<bool> res_ndb;

  for(int i = 0; i < TUPLE_NUM; i++)
  {
    res_cal.push_back(false);
    res_ndb.push_back(false);
  }

  check_all_tuples(str, res_cal.getBase());
  ndbapi_tuples(ndb, str, res_ndb.getBase());

  for(int i = 0; i < TUPLE_NUM; i++)
  {
    if(res_cal[i] != res_ndb[i])
      return false;
  }
  return true;
}


int runCreateTables(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb *pNdb = GETNDB(step);
  pNdb->getDictionary()->dropTable(MYTAB1.getName());
  int ret = createTable(pNdb, &MYTAB1, false, true, 0); 
  if(ret)
    return ret;
  return NDBT_OK;
}


int runDropTables(NDBT_Context* ctx, NDBT_Step* step)
{
 int ret = GETNDB(step)->getDictionary()->dropTable(MYTAB1.getName());
 if(ret == -1)
   return NDBT_FAILED;
  
  return NDBT_OK;
}

int runScanRandomFilterTest(NDBT_Context* ctx, NDBT_Step* step)
{
  char random_str[MAX_STR_LEN];
  Ndb *myNdb = GETNDB(step);
  bool res = true;

  for(int i = 0; i < TEST_NUM; i++)
  {
    get_rand_op_str_compound(random_str);
    if( !compare_cal_ndb(random_str, myNdb)) 
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

NDBT_TESTSUITE(testScanFilter);
TESTCASE(TEST_NAME, 
	 "Scan table TABLE_NAME for the records which accord with \
   conditions of logical scan operations: AND/OR/NAND/NOR")
{
  INITIALIZER(runCreateTables);
  INITIALIZER(runPopulate);
  INITIALIZER(runScanRandomFilterTest);
  FINALIZER(runDropTables);
}

NDBT_TESTSUITE_END(testScanFilter);


int main(int argc, const char** argv)
{
  ndb_init();

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1))
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  return testScanFilter.executeOneCtx(con, &MYTAB1, TEST_NAME);
}
