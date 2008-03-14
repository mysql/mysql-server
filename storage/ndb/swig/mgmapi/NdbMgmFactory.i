/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

%newobject NdbMgmFactory::createNdbMgm;

%{

  class NdbMgmFactory
  {

  public:
    static ndb_mgm_handle * createNdbMgm(const char * connectString = 0)
      {
        ndb_mgm_handle * handle = ndb_mgm_create_handle();
        if (handle == NULL) {
          return NULL;
        }
        if (connectString != 0) {
          int ret = ndb_mgm_set_connectstring(handle,connectString);
          if (ret == -1) {
            free(handle);
            return NULL;
          }
        }
        return handle;
      }
  };

  %}


class NdbMgmFactory
{
  // We list these here as private so that SWIG doesnt generate them
  NdbMgmFactory();
  ~NdbMgmFactory();
public:

  %ndbexception("NdbMgmException") {
    $action
      if (result==NULL) {
        NDB_exception(NdbMgmException,"Couldn't allocate NdbMgm");
      }
  }
  static ndb_mgm_handle * createNdbMgm(const char * connectString = 0);
};

