if (AERON_SYSTEM_TESTS)
    add_test(
        NAME java_system_tests_c_media_driver
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ./gradlew -Daeron.test.system.aeronmd.path=$<TARGET_FILE:aeronmd> :aeron-system-tests:cleanTest :aeron-system-tests:test --no-daemon)
endif ()

if (AERON_SLOW_SYSTEM_TESTS)
    add_test(
        NAME java_slow_system_tests_c_media_driver
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ./gradlew -Daeron.test.system.aeronmd.path=$<TARGET_FILE:aeronmd> :aeron-system-tests:cleanTest :aeron-system-tests:slowTest --no-daemon)
endif ()
