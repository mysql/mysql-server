/* Copyright (C) 2005 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Structure of the name list */

struct show_table_contributors_st {
  const char *name;
  const char *location;
  const char *comment;
};

/*
  Output from "SHOW CONTRIBUTORS"

  Get permission before editing.

  IMPORTANT: Names should be left in historical order.

  Names should be encoded using UTF-8.
*/

struct show_table_contributors_st show_table_contributors[]= {
  {"Ronald Bradford", "Brisbane, Australia", "EFF contribution for UC2006 Auction"},
  {"Sheeri Kritzer", "Boston, Mass. USA", "EFF contribution for UC2006 Auction"},
  {"Mark Shuttleworth", "London, UK.", "EFF contribution for UC2006 Auction"},
  {NULL, NULL, NULL}
};
