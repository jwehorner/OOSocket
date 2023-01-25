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

TEST(SocketTest, Test_UDPSocket_constructor_invalid) {
	bool exception_thrown = false;
	try
	{
		// Invalid port and any address
		UDPSocket s = UDPSocket(10);
		exception_thrown = false;
	}
	catch (std::exception e)
	{
		exception_thrown = true;
	}
	ASSERT_TRUE(exception_thrown);

	try
	{
		// Valid port and invalid address.
		UDPSocket s = UDPSocket(44444, "0.0.0.0");
		exception_thrown = false;
	}
	catch (std::exception e)
	{
		exception_thrown = true;
	}
	ASSERT_TRUE(exception_thrown);
}