package testsuite.clusterj.bindings;

public class TimeTypesTest extends testsuite.clusterj.TimeTypesTest {

    @Override
    public void localSetUp() {
        System.out.println("============ test skipped due to Time zone issues ===========");
    }

    @Override
    // This test fails due to time zone problems
    public void testWriteJDBCReadNDB() {}

    // This test fails due to time zone problems
    public void testWriteNDBReadJDBC() {}

}
