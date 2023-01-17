/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import testsuite.clusterj.model.Employee;
import testsuite.clusterj.model.LongIntStringPK;

import com.mysql.clusterj.ClusterJUserException;

public class PartitionKeyTest extends AbstractClusterJTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        addTearDownClasses(Employee.class, LongIntStringPK.class);
    }

    public void test() {
        badClass();
        wrongKeyTypePrimitive();
        wrongKeyTypePrimitiveNull();
        wrongKeyTypeCompound();
        wrongKeyTypeCompoundNull();
        wrongKeyTypeCompoundNullPart();
        setPartitionKeyTwice();
        goodIntKey();
        goodCompoundKey();
        session = sessionFactory.getSession(); // to allow tear down classes to work
        failOnError();
    }

    protected void badClass() {
        try {
            session = sessionFactory.getSession();
            session.setPartitionKey(Integer.class, 0);
            error("Failed to throw exception on setPartitionKey(Integer.class, 0)");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void wrongKeyTypePrimitive() {
        try {
            session = sessionFactory.getSession();
            session.setPartitionKey(Employee.class, 0L);
            error("Failed to throw exception on setPartitionKey(Employee.class, 0L)");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void wrongKeyTypePrimitiveNull() {
        try {
            session = sessionFactory.getSession();
            session.setPartitionKey(Employee.class, null);
            error("Failed to throw exception on setPartitionKey(Employee.class, null)");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void wrongKeyTypeCompound() {
        try {
            session = sessionFactory.getSession();
            session.setPartitionKey(LongIntStringPK.class, 0L);
            error("Failed to throw exception on setPartitionKey(LongIntStringPK.class, 0L)");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void wrongKeyTypeCompoundPart() {
        try {
            Object[] key = new Object[] {0L, 0L, ""};
            session = sessionFactory.getSession();
            session.setPartitionKey(LongIntStringPK.class, key);
            error("Failed to throw exception on setPartitionKey(LongIntStringPK.class, new Object[] {0L, 0L, \"\"})");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void wrongKeyTypeCompoundNull() {
        try {
            session = sessionFactory.getSession();
            session.setPartitionKey(LongIntStringPK.class, null);
            error("Failed to throw exception on setPartitionKey(LongIntStringPK.class, null)");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void wrongKeyTypeCompoundNullPart() {
        try {
            session = sessionFactory.getSession();
            Object[] key = new Object[] {0L, null, ""};
            session.setPartitionKey(LongIntStringPK.class, key);
            error("Failed to throw exception on setPartitionKey(LongIntStringPK.class, new Object[] {0L, null, \"\"})");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void setPartitionKeyTwice() {
        try {
            session = sessionFactory.getSession();
            // partition key cannot be null
            Object[] key = new Object[] {0L, 0, ""};
            session.setPartitionKey(LongIntStringPK.class, key);
            session.setPartitionKey(LongIntStringPK.class, key);
            error("Failed to throw exception on second setPartitionKey");
        } catch (ClusterJUserException ex){
            // good catch
        } finally {
            session.close();
            session = null;
        }
    }

    protected void goodIntKey() {
        try {
            session = sessionFactory.getSession();
            session.deletePersistentAll(Employee.class);
            Employee employee = session.newInstance(Employee.class);
            employee.setId(1000);
            employee.setAge(1000);
            employee.setMagic(1000);
            employee.setName("Employee 1000");
            session.setPartitionKey(Employee.class, 1000);
            session.makePersistent(employee);
        } finally {
        session.close();
        session = null;
    }
    }

    protected void goodCompoundKey() {
        try {
            session = sessionFactory.getSession();
            session.deletePersistentAll(LongIntStringPK.class);
            // key can contain nulls if not part of partition key
            Object[] key = new Object[] { 1000L, 1000, null};
            LongIntStringPK instance = session
                    .newInstance(LongIntStringPK.class);
            instance.setLongpk(1000L);
            instance.setIntpk(1000);
            instance.setStringpk("1 Thousand");
            session.setPartitionKey(LongIntStringPK.class, key);
            session.makePersistent(instance);
        } finally {
            session.close();
            session = null;
        }
    }

}
