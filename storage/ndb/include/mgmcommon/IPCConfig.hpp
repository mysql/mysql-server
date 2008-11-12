/* Copyright (C) 2003-2008 MySQL AB, Sun Microsystems Inc.

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

#ifndef IPCConfig_H
#define IPCConfig_H

struct IPCConfig
{
  /*
    configure_transporters

    Create and configure transporters in TransporterRegistry

    Returns:
      true  - sucessfully created and (re)configured transporters
      false - at least one transporter could not be created
              or (re)configured
  */
  static bool configureTransporters(Uint32 nodeId,
                                    const struct ndb_mgm_configuration &,
                                    class TransporterRegistry &);
};

#endif // IPCConfig_H
