cmake_minimum_required(VERSION 3.14)

# Commission unit tests
add_executable(CommissionsManagerTest CommissionsManagerTest.cpp)
target_link_libraries(CommissionsManagerTest
    gtest_main
    rates
    logger
    settings
    common
)

add_executable(TopologyTrustLinesManagerCommissionsTest TopologyTrustLinesManagerCommissionsTest.cpp)
target_link_libraries(TopologyTrustLinesManagerCommissionsTest
    gtest_main
    topology
    logger
    contractors
    common
)

# Add tests to the test suite
add_test(NAME CommissionsManagerTest COMMAND CommissionsManagerTest)
add_test(NAME TopologyTrustLinesManagerCommissionsTest COMMAND TopologyTrustLinesManagerCommissionsTest)