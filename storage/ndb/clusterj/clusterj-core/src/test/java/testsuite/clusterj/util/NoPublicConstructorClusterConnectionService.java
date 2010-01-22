package testsuite.clusterj.util;

import com.mysql.clusterj.core.store.ClusterConnection;

public class NoPublicConstructorClusterConnectionService 
        implements com.mysql.clusterj.core.store.ClusterConnectionService {

    private NoPublicConstructorClusterConnectionService() {}

    public ClusterConnection create(String connectString) {
        return null;
    }

}
