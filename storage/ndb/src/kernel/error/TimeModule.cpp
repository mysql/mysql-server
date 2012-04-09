/*
   Copyright (C) 2003-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

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



#include <ndb_global.h>
#include "TimeModule.hpp"

static const char* cMonth[]  = { "x", "January", "February", "March", "April", "May", "June",
				 "July", "August", "September", "October", "November", "December"};

static const char* cDay[]    = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
				 "Saturday", "Sunday"};

static const char* cHour[]   = { "00","01","02","03","04","05","06","07","08","09","10","11","12",
				 "13","14","15","16","17","18","19","20","21","22","23"};

static const char* cMinute[] = { "00","01","02","03","04","05","06","07","08","09","10","11","12",
				 "13","14","15","16","17","18","19","20","21","22","23","24","25",
				 "26","27","28","29","30","31","32","33","34","35","36","37","38",
				 "39","40","41","42","43","44","45","46","47","48","49","50","51",
				 "52","53","54","55","56","57","58","59"};

static const char* cSecond[] = { "00","01","02","03","04","05","06","07","08","09","10","11","12",
				 "13","14","15","16","17","18","19","20","21","22","23","24","25",
				 "26","27","28","29","30","31","32","33","34","35","36","37","38",
				 "39","40","41","42","43","44","45","46","47","48","49","50","51",
				 "52","53","54","55","56","57","58","59"};


TimeModule::TimeModule(){
}

TimeModule::~TimeModule(){
}

void
TimeModule::setTimeStamp()
{
   struct tm* rightnow;
   time_t now;

   time(&now);

   rightnow = localtime(&now);

   iYear     = rightnow->tm_year+1900; // localtime returns current year -1900
   iMonth    = rightnow->tm_mon+1;     // and month 0-11
   iMonthDay = rightnow->tm_mday;
   iWeekDay  = rightnow->tm_wday;
   iHour     = rightnow->tm_hour;
   iMinute   = rightnow->tm_min;
   iSecond   = rightnow->tm_sec;
}

int
TimeModule::getYear() const
{
  return iYear;
}

int
TimeModule::getMonthNumber() const
{
  return iMonth;
}

const char* 
TimeModule::getMonthName() const {
  return cMonth[iMonth];
}

int 
TimeModule::getDayOfMonth() const {
  return iMonthDay;
}

const char* 
TimeModule::getDayName() const {
  return cDay[iWeekDay];
}

const char* 
TimeModule::getHour() const { 
    return cHour[iHour];
}

const char* 
TimeModule::getMinute() const {
  return cMinute[iMinute];
}

const char* 
TimeModule::getSecond() const {
  return cSecond[iSecond];
}
