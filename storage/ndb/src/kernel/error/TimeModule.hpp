/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _TimeModule_
#define _TimeModule_

#define JAM_FILE_ID 486


class TimeModule {
public:
  TimeModule();
  ~TimeModule();
  
  void setTimeStamp();
  
  int getYear()              const;
  int getMonthNumber()       const;
  int getDayOfMonth()        const;
  const char* getMonthName() const;
  const char* getDayName()   const;
  const char* getHour()      const;
  const char* getMinute()    const;
  const char* getSecond()    const;

private:
  int iYear;
  int iMonth;
  int iMonthDay;
  int iWeekDay;
  int iHour;
  int iMinute;
  int iSecond;
};


#undef JAM_FILE_ID

#endif
