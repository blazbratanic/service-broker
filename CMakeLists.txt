cmake_minimum_required(VERSION 2.8.8)
set(PROJECT_NAME_STR communication)
project(${PROJECT_NAME_STR} CXX)


set(SRC_FILES ${PROJECT_SOURCE_DIR}/src/communication.cpp)

set(HEADER_FILES 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/Concat.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/Combiner.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/Log.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/ServiceBroker.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/Service.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/WorkerBase.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/WorkerMultiThreaded.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/ContextBase.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/WorkerSingleThreaded.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/Workers.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/contains_type.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/convert_to_tuple.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/get_element_by_type.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/noncopyable.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/to_function.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/traits.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/type_constraints.hpp 
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/detail/tuple_util.hpp) 


add_library(${PROJECT_NAME_STR} ${SRC_FILES} ${HEADER_FILES})
set_target_properties(${PROJECT_NAME_STR} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${PROJECT_NAME} 
    PRIVATE ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME_STR}
    ${Boost_INCLUDE_DIRS}
    PUBLIC  ${cxml_SOURCE_DIR}/include
    ${threadpool_SOURCE_DIR}/include)

if (build_tests)
    enable_testing()
    add_subdirectory(${PROJECT_SOURCE_DIR}/test)
endif()
