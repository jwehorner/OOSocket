##########################################
# Google Test Installation
##########################################
Include(FetchContent)

FetchContent_Declare(
	Catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG        v3.3.2
)

FetchContent_MakeAvailable(Catch2)

enable_testing()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")

##########################################
# Configure Directory Header
##########################################

##########################################
# Catch Test Targets
##########################################
add_executable(test_udp_socket			"${CMAKE_SOURCE_DIR}/test/test_udp_socket.cpp")

include_directories(test_udp_socket		"${SOCKET_INCLUDES_LIST}")

target_link_libraries(test_udp_socket 	Catch2::Catch2WithMain)

if(WIN32)
  	target_link_libraries(test_udp_socket	wsock32 ws2_32)
endif()

##########################################
# Regular Test Targets
##########################################