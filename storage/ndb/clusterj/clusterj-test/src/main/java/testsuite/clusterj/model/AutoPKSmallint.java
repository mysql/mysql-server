/*
 *  Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.PersistenceCapable;

/** Schema

drop table if exists autopksmallint;
create table autopksmallint (
  id smallint primary key auto_increment,
  val smallint
) ENGINE=ndb;

 */
@PersistenceCapable(table="autopksmallint")
public interface AutoPKSmallint {

    short getId();
    void setId(short id);

    Short getVal();
    void setVal(Short val);

}
