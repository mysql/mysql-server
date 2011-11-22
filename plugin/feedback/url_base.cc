/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

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

#include "feedback.h"

namespace feedback {

Url* http_create(const char *url, size_t url_length);

/**
  creates an Url object out of an url, if possible.

  This is done by invoking corresponding creator functions
  of the derived classes, until the first not NULL result.
*/
Url* Url::create(const char *url, size_t url_length)
{
  url= my_strndup(url, url_length, MYF(MY_WME));
  
  if (!url)
    return NULL;

  Url *self= http_create(url, url_length);

  /*
    here we can add

    if (!self) self= smtp_create(url, url_length);
    if (!self) self= tftp_create(url, url_length);
    etc
  */

  if (!self)
    my_free(const_cast<char*>(url));

  return self;
}

} // namespace feedback
