/* Copyright (C) 2003 MySQL AB

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


#include <ndb_global.h>

#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NDBT_Tables.hpp>
#include <getarg.h>
#include <NDBT.hpp>
#include <HugoCalculator.hpp>

//extern NdbOut g_info;

int main(int argc, const char** argv)
{
  ndb_init();
  int _row = 0;
  int _column = 0;
  int _updates = 0;
  const char* _tableName = NULL;

  struct getargs args[] = {
    { "row", 'r', arg_integer, &_row, "The row number", "row" },
    { "column", 'c', arg_integer, &_column, "The column id", "column" },
    { "updates", 'u', arg_integer, &_updates, "# of updates", "updates" }
  };

  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;

  if(getarg(args, num_args, argc, argv, &optind) || argv[optind] == NULL) {
    arg_printusage(args, num_args, argv[0], "table name\n");
    return NDBT_WRONGARGS;
  }
  // Check if table name is supplied
  if (argv[optind] != NULL) 
    _tableName = argv[optind];


  const NdbDictionary::Table* table = NDBT_Tables::getTable(_tableName);
  const NdbDictionary::Column * attribute = table->getColumn(_column);

  g_info << "Table " << _tableName << endl
	 << "Row: " << _row << ", "
	 << "Column(" << attribute->getName() << ")"
	 << "[" << attribute->getType() << "]" 
	 << ", Updates: " << _updates
	 << endl;

  HugoCalculator calc(*table);
  char buf[8000];
  g_info << "Value: " << calc.calcValue(_row, _column, _updates, buf)
	 << endl;

  return 0;
}
