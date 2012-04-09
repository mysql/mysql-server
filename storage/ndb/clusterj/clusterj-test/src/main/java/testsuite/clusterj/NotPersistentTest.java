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

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.QueryBuilder;

import java.util.ArrayList;
import java.util.List;

import testsuite.clusterj.model.NotPersistentTypes;

public class NotPersistentTest extends AbstractClusterJModelTest {

    private static final int NUMBER_TO_INSERT = 5;

    protected List<NotPersistentTypes> notPersistentTypes =
            new ArrayList<NotPersistentTypes>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createNotPersistentTypesInstances(NUMBER_TO_INSERT);
        int count = session.deletePersistentAll(NotPersistentTypes.class);
        addTearDownClasses(NotPersistentTypes.class);
//        System.out.println("Deleted " + count + " instances.");
    }

    public void test() {
        defaultValues();
        setValues();
        insert();
        find();
        query();
        badQuery();
        delete();
        failOnError();
    }

    protected void defaultValues() {
        NotPersistentTypes npt = session.newInstance(NotPersistentTypes.class);
        errorIfNotEqual("NotPersistentInteger default value.",
                null, npt.getNotPersistentInteger());
        errorIfNotEqual("NotPersistentChildren default value.",
                null, npt.getNotPersistentChildren());
        errorIfNotEqual("NotPersistentInt default value.",
                0, npt.getNotPersistentInt());
        errorIfNotEqual("Id default value.",
                0, npt.getId());
        errorIfNotEqual("Name default value.",
                null, npt.getName());
        errorIfNotEqual("Magic default value.",
                0, npt.getMagic());
        errorIfNotEqual("Age default value.",
                null, npt.getAge());
    }

    protected void setValues() {
        NotPersistentTypes npt = notPersistentTypes.get(0);
        List<NotPersistentTypes> children = new ArrayList<NotPersistentTypes>();
        children.add(notPersistentTypes.get(1));
        children.add(notPersistentTypes.get(2));
        npt.setNotPersistentChildren(children);
        errorIfNotEqual("NotPersistentInteger default value.",
                children, npt.getNotPersistentChildren());
        int intValue = 666;
        npt.setNotPersistentInt(intValue);
        errorIfNotEqual("NotPersistentChildren default value.",
                intValue, npt.getNotPersistentInt());
        Integer integerValue = Integer.valueOf(intValue);
        npt.setNotPersistentInteger(integerValue);
        errorIfNotEqual("NotPersistentInt default value.",
                integerValue, npt.getNotPersistentInteger());
    }

    protected void insert() {
        tx = session.currentTransaction();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            session.makePersistent(notPersistentTypes.get(i));
        }

        tx.commit();
    }

    protected void find() {
        tx = session.currentTransaction();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            NotPersistentTypes instance = session.find(NotPersistentTypes.class, i);
            errorIfNotEqual("Wrong instance", i, instance.getId());
        }
        tx.commit();
    }

    protected void query() {
        tx = session.currentTransaction();
        tx.begin();
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType dobj = builder.createQueryDefinition(NotPersistentTypes.class);
        dobj.where(dobj.get("id").equal(dobj.param("id")));
        Query query = session.createQuery(dobj);
        query.setParameter("id", 0);
        List<NotPersistentTypes> result = query.getResultList();
        int resultSize = result.size();
        errorIfNotEqual("Wrong query result size", 1, resultSize);
        NotPersistentTypes npt = result.get(0);
        int resultId = npt.getId();
        tx.commit();
        errorIfNotEqual("Wrong query result instance id 0", 0, resultId);
    }

    protected void badQuery() {
        try {
            QueryBuilder builder = session.getQueryBuilder();
            QueryDomainType dobj = builder.createQueryDefinition(NotPersistentTypes.class);
            dobj.where(dobj.get("notPersistentChildren").equal(dobj.param("id")));
            error("Failed to catch user exception for not-persistent query field");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }

    protected void delete() {
        tx = session.currentTransaction();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            session.deletePersistent(notPersistentTypes.get(i));
        }
        tx.commit();
    }

    protected void createNotPersistentTypesInstances(int count) {
        notPersistentTypes = new ArrayList<NotPersistentTypes>(count);
        for (int i = 0; i < count; ++i) {
            NotPersistentTypes npt = session.newInstance(NotPersistentTypes.class);
            npt.setId(i);
            npt.setName("Employee number " + i);
            npt.setAge(i);
            npt.setMagic(i);
            notPersistentTypes.add(npt);
        }
    }

}
