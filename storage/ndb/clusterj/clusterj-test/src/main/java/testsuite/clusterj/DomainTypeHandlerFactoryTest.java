/*
   Copyright 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

import testsuite.clusterj.model.CrazyDelegate;
import testsuite.clusterj.model.ThrowNullPointerException;

/**
 * This test verifies the behavior of the core implementation to provide for
 * loadable DomainTypeHandlerFactory implementations. This is the way to
 * support abstract classes or concrete classes as domain types.
 * 
 * If a file META-INF/services/com.mysql.clusterj.spi.DomainTypeHandlerFactory
 * exists that names a loadable class, the core implementation should use it
 * preferentially to load a DomainTypeHandler for an unregistered domain type.
 *
 * This test includes an implementation of DomainTypeHandlerFactory, called
 * CrazyDomainTypeHandlerFactoryImpl, that has three modes of operation:
 * <ol><li> if the simple domain class name begins with "Throw", then an
 * exception is thrown immediately. This should cause clusterj to save the
 * exception information and proceed to the next factory. The test will succeed
 * when the standard implementation does not accept the domain class as a
 * valid domain class.
 * </li><li> if the simple domain class name is "CrazyDelegate", then an
 * implementation of DomainTypeHandlerFactory is returned. The returned
 * implementation doesn't immediately throw an exception but throws an
 * exception when the method getProxyClass is called. This exception will be
 * reported to clusterj and the test succeeds.
 * </li><li> in all other cases, null is returned. This should cause clusterj
 * to go to the next prospective factory. This case is exercised by every test
 * case that includes the CrazyDomainTypeHandlerFactoryImpl in the services
 * file (i.e. all other test cases).
 * </li></ol>
 */
public class DomainTypeHandlerFactoryTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
    }

    public void testThrowNullPointerException() {
        try {
            ThrowNullPointerException instance = new ThrowNullPointerException();
            // This should cause CrazyDomainTypeHandlerFactoryImpl to throw
            // a NullPointerException which should be ignored. But the standard
            // implementation doesn't like ThrowNullPointerException either,
            // so it should throw an exception. But the error message should
            // contain the original NullPointerException that was thrown by
            // CrazyDomainTypeHandlerFactoryImpl.
            session.makePersistent(instance);
            error("Failed to catch RuntimeException");
        } catch (RuntimeException e) {
            if (!e.getMessage().contains("java.lang.NullPointerException")) {
                error("Failed to catch correct exception, but caught: " + e.toString());
            }
            // good catch
        }
        failOnError();
    }

    public void testCrazyDelegate() {
        try {
            CrazyDelegate instance = new CrazyDelegate();
            // This should fail at CrazyDomainTypeHandlerFactoryImpl.getProxyClass()
            // It's a bit of a cheat since it relies on the internal implementation
            // of SessionFactoryImpl.getDomainTypeHandler
            session.makePersistent(instance);
            error("Failed to catch UnsupportedOperationException");
        } catch (UnsupportedOperationException ex) {
            String message = ex.getMessage();
            Throwable cause = ex.getCause();
            String causeMessage =  cause==null?"null":cause.getMessage();
            if (!message.contains("Nice Job!")) {
                error("Failed to catch correct exception, but caught: " + message + "; cause: " + causeMessage);
            }
        }
        failOnError();
    }
}
