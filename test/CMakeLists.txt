set(tests service_broker_test.cpp worker_test.cpp multithreaded_worker_test.cpp
    concat_test.cpp)

enable_testing()

foreach(test_cpp_file ${tests})
    get_filename_component(test_name ${test_cpp_file} NAME_WE)
    add_executable(${test_name} ${test_cpp_file} ${lang_src})
    add_dependencies(${test_name} gtest gmock gmock_main)
    set_property(TARGET ${test_name} PROPERTY FOLDER "${TEST_FOLDER}")

    target_include_directories(${test_name} PRIVATE
        ${gtest_SOURCE_DIR}/include
        ${gmock_SOURCE_DIR}/include
        ${communication_SOURCE_DIR}/include
        ${cxml_SOURCE_DIR}/include
        ${threadpool_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS})

    target_link_libraries(${test_name}
        gmock
        gmock_main
        cxml)

    if (NOT MSVC)
        TARGET_LINK_LIBRARIES(${test_name} dl pthread)
    endif()

    add_test(${test_name} ${test_name})
endforeach()
