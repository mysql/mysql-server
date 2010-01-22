package testsuite.clusterj;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.core.store.ClusterConnectionService;
public class NegativeClusterConnectionServicePropertyTest extends AbstractClusterJTest {

    public void testBadClassName() {
        try {
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class, "some.random.name");
            fail("Expected ClassNotFoundException, got no exception");
        } catch (ClusterJFatalUserException e) {
            // make sure the enclosed exception is ClassNotFoundException
            Throwable cause = e.getCause();
            if (!(cause instanceof ClassNotFoundException)) {
                fail("Expected ClassNotFoundException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

    public void testClassNotClusterConnectionService() {
        try {
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class, "testsuite.clusterj.util.DoesNotImplementClusterConnectionService");
            fail("Expected ClassCastException, got no exception");
        } catch (ClusterJFatalUserException e) {
            // make sure the enclosed exception is ClassCastException
            Throwable cause = e.getCause();
            if (!(cause instanceof ClassCastException)) {
                fail("Expected ClassCastException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

    public void testNotPublicConstructorClusterConnectionService() {
        try {
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class, "testsuite.clusterj.util.NoPublicConstructorClusterConnectionService");
            fail("Expected IllegalAccessException, got no exception");
        } catch (ClusterJFatalUserException e) {
            // make sure the enclosed exception is IllegalAccessException
            Throwable cause = e.getCause();
            if (!(cause instanceof IllegalAccessException)) {
                fail("Expected IllegalAccessException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

}
