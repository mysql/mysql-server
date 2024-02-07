/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <time.h>

#include <portlib/ndb_localtime.h>
#include "TimeModule.hpp"

#define JAM_FILE_ID 488

static const char *cMonth[] = {
    "x",    "January", "February",  "March",   "April",    "May",     "June",
    "July", "August",  "September", "October", "November", "December"};

static const char *cDay[] = {"Sunday",   "Monday", "Tuesday",  "Wednesday",
                             "Thursday", "Friday", "Saturday", "Sunday"};

static const char *cHour[] = {"00", "01", "02", "03", "04", "05", "06", "07",
                              "08", "09", "10", "11", "12", "13", "14", "15",
                              "16", "17", "18", "19", "20", "21", "22", "23"};

static const char *cMinute[] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11",
    "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35",
    "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59"};

static const char *cSecond[] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11",
    "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35",
    "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59"};

TimeModule::TimeModule() {}

TimeModule::~TimeModule() {}

void TimeModule::setTimeStamp() {
  time_t now;
  time(&now);

  tm tm_buf;
  ndb_localtime_r(&now, &tm_buf);

  iYear = tm_buf.tm_year + 1900;  // localtime returns current year -1900
  iMonth = tm_buf.tm_mon + 1;     // and month 0-11
  iMonthDay = tm_buf.tm_mday;
  iWeekDay = tm_buf.tm_wday;
  iHour = tm_buf.tm_hour;
  iMinute = tm_buf.tm_min;
  iSecond = tm_buf.tm_sec;
}

int TimeModule::getYear() const { return iYear; }

int TimeModule::getMonthNumber() const { return iMonth; }

const char *TimeModule::getMonthName() const { return cMonth[iMonth]; }

int TimeModule::getDayOfMonth() const { return iMonthDay; }

const char *TimeModule::getDayName() const { return cDay[iWeekDay]; }

const char *TimeModule::getHour() const { return cHour[iHour]; }

const char *TimeModule::getMinute() const { return cMinute[iMinute]; }

const char *TimeModule::getSecond() const { return cSecond[iSecond]; }
