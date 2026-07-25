class DebugRunner {
    static void runGroup(String name, Runnable fn) {
        TestSpoiBoundary.passed = 0;
        TestSpoiBoundary.failed = 0;
        fn.run();
        if (TestSpoiBoundary.failed > 0) {
            System.out.println("FAIL: " + name + " passed=" + TestSpoiBoundary.passed + " failed=" + TestSpoiBoundary.failed);
        }
    }
    
    public static void main(String[] args) {
        runGroup("testUnicodeAttack", TestSpoiBoundary::testUnicodeAttack);
        runGroup("testPipeTypeChange", TestSpoiBoundary::testPipeTypeChange);
        runGroup("testNestedMap", TestSpoiBoundary::testNestedMap);
        runGroup("testIntegerOverflow", TestSpoiBoundary::testIntegerOverflow);
        runGroup("testFilterWithPath", TestSpoiBoundary::testFilterWithPath);
        runGroup("testSortComplex", TestSpoiBoundary::testSortComplex);
        runGroup("testDistinctMixed", TestSpoiBoundary::testDistinctMixed);
        runGroup("testResetNested", TestSpoiBoundary::testResetNested);
        runGroup("testExtremeStream", TestSpoiBoundary::testExtremeStream);
        runGroup("testAttackChain", TestSpoiBoundary::testAttackChain);
        runGroup("testNullByteAttack", TestSpoiBoundary::testNullByteAttack);
        runGroup("testKeysValuesChain", TestSpoiBoundary::testKeysValuesChain);
        runGroup("testMultiExecutor", TestSpoiBoundary::testMultiExecutor);
    }
}