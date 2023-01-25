##########################################
# Google Test Installation
##########################################
include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")

##########################################
# Configure Directory Header
##########################################

##########################################
# Google Test Targets
##########################################
add_executable(test_udp_socket			"${CMAKE_SOURCE_DIR}/test/test_udp_socket.cpp")

include_directories(test_udp_socket		"${SOCKET_INCLUDES_LIST}")

target_link_libraries(test_udp_socket 	GTest::gtest_main)

if(WIN32)
  target_link_libraries(test_udp_socket	wsock32 ws2_32)
endif()

##########################################
# Regular Test Targets
##########################################
include(GoogleTest)
gtest_discover_tests(test_udp_socket)
