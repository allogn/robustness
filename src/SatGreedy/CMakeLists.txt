cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 11)
project (SatGreedy)
enable_testing()


set(DIM ${CMAKE_SOURCE_DIR}/include/dim/dim.cpp)

set(DAGGER_SOURCE_FILES ${CMAKE_SOURCE_DIR}/include/DGRAIL/Graph.cpp
        ${CMAKE_SOURCE_DIR}/include/DGRAIL/Vertex.cpp
        ${CMAKE_SOURCE_DIR}/include/DGRAIL/Grail.cpp
        ${CMAKE_SOURCE_DIR}/include/DGRAIL/DagWrapper.cpp)



find_package(Boost COMPONENTS unit_test_framework filesystem system program_options timer REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

include_directories(${CMAKE_SOURCE_DIR}/include/)
add_executable(SatGreedy satgreedy.cpp ${CMAKE_SOURCE_DIR}/include/sfmt/SFMT.c ${DIM})
target_link_libraries(SatGreedy ${Boost_LIBRARIES})

add_executable(Imm imm.cpp ${CMAKE_SOURCE_DIR}/include/sfmt/SFMT.c ${DIM})
target_link_libraries(Imm ${Boost_LIBRARIES})

add_executable(Simulator simulator.cpp ${CMAKE_SOURCE_DIR}/include/sfmt/SFMT.c)
target_link_libraries(Simulator ${Boost_LIBRARIES})


add_executable(Rob rob.cpp ${CMAKE_SOURCE_DIR}/include/sfmt/SFMT.c ${DAGGER_SOURCE_FILES})
target_link_libraries(Rob ${Boost_LIBRARIES})

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/*.cpp)

set_target_properties(Imm PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/build)
set_target_properties(SatGreedy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/build)
set_target_properties(Simulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/build)
set_target_properties(Rob PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/build)

#Run through each source
foreach(testSrc ${TEST_SRCS})
        #Extract the filename without an extension (NAME_WE)
        get_filename_component(testName ${testSrc} NAME_WE)

        #Add compile target
        add_executable(${testName} ${testSrc} ${CMAKE_SOURCE_DIR}/include/sfmt/SFMT.c ${DAGGER_SOURCE_FILES} ${DIM})

        #link to Boost libraries AND your targets and dependencies
        target_link_libraries(${testName} ${Boost_LIBRARIES})

        #I like to move testing binaries into a testBin directory
        set_target_properties(${testName} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/build/testBin)

        #Finally add it to test execution -
        #Notice the WORKING_DIRECTORY and COMMAND
        add_test(NAME ${testName}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/testBin
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/testBin/${testName} )
endforeach(testSrc)
