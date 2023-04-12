#include <iostream>
#include <stdio.h>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "UDPSocket.hpp"

TEST_CASE("Check constructor under valid conditions.", "[UDPSocket]") {
	// Valid port and any address
	REQUIRE_NOTHROW(UDPSocket(44444));
	// Valid port and localhost
	REQUIRE_NOTHROW(UDPSocket(55555, "127.0.0.1"));
}

TEST_CASE("Check receive with timeout.", "[UDPSocket]") {
	// Valid port and any address
	std::shared_ptr<UDPSocket> s1;
	REQUIRE_NOTHROW(s1 = std::make_shared<UDPSocket>(66666));
	REQUIRE_NOTHROW(s1->set_socket_receive_timeout(1000));
	REQUIRE(s1->receive().size() == 0);
}

TEST_CASE("Check send and receive without timeout.", "[UDPSocket]") {
	std::shared_ptr<UDPSocket> s1;
	std::shared_ptr<UDPSocket> s2;
	REQUIRE_NOTHROW(s1 = std::make_shared<UDPSocket>(16666));
	REQUIRE_NOTHROW(s2 = std::make_shared<UDPSocket>());

	SECTION("Send data from s2 to s1 in a lambda.") {
		std::thread send_thread = std::thread([s2]() {
			std::vector<char> buffer = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\0'};
			s2->send_to(buffer, 16666);
		});
		REQUIRE(std::string(s1->receive().data()).compare("hello world!") == 0);
		send_thread.join();
	}

	SECTION("Configure the remote host for s2 then send in a lambda.") {
		REQUIRE_NOTHROW(s2->configure_remote_host(16666));
		std::thread send_thread = std::thread([s2]() {
			std::vector<char> buffer = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\0'};
			s2->send(buffer);
		});
		REQUIRE(std::string(s1->receive().data()).compare("hello world!") == 0);
		send_thread.join();
	}
}