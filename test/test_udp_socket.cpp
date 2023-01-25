#include <stdio.h>
#include <vector>

#include <gtest/gtest.h>

#include "UDPSocket.hpp"

using namespace std;

TEST(SocketTest, Test_UDPSocket_constructor_valid) {
	bool exception_thrown = false;
	try
	{
		// Valid port and any address
		UDPSocket s1 = UDPSocket(44444);
		// Valid port and localhost
		UDPSocket s2 = UDPSocket(55555, "127.0.0.1");
		exception_thrown = false;
	}
	catch (std::exception e)
	{
		exception_thrown = true;
	}
	ASSERT_FALSE(exception_thrown);
}

TEST(SocketTest, Test_UDPSocket_receive_timeout) {
	bool exception_thrown = false;
	try
	{
		// Valid port and any address
		UDPSocket s1 = UDPSocket(66666);
		s1.set_socket_receive_timeout(1000);
		std::vector<char> data = s1.receive();
		ASSERT_EQ(data.size(), 0);
		exception_thrown = false;
	}
	catch (std::exception e)
	{
		exception_thrown = true;
	}
	ASSERT_FALSE(exception_thrown);
}
