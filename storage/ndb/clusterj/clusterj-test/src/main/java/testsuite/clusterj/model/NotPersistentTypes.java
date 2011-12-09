/*
   Copyright 2010 Sun Microsystems, Inc.
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

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.NotPersistent;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;
import java.util.Collection;

/** This interface is based on Employee but adds not-persistent properties.
 */
@PersistenceCapable(table="t_basic")
public interface NotPersistentTypes extends IdBase {
    
    @PrimaryKey
    int getId();
    void setId(int id);

    String getName();
    void setName(String name);

    @Index(name="idx_unique_hash_magic")
    int getMagic();
    void setMagic(int magic);

    @Index(name="idx_btree_age")
    Integer getAge();
    void setAge(Integer age);

    @NotPersistent
    Collection<NotPersistentTypes> getNotPersistentChildren();
    void setNotPersistentChildren(Collection<NotPersistentTypes> value);

    @NotPersistent
    int getNotPersistentInt();
    void setNotPersistentInt(int value);

    @NotPersistent
    Integer getNotPersistentInteger();
    void setNotPersistentInteger(Integer value);

}
