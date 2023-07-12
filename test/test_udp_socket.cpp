#include <iostream>
#include <stdio.h>
#include <memory>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark_all.hpp>

#include "udp_socket.hpp"

TEST_CASE("Check constructor under valid conditions.", "[socket::udp::socket][test]") {
	// Valid port and any address
	REQUIRE_NOTHROW(oo_socket::udp::socket(44444));
	// Valid port and localhost
	REQUIRE_NOTHROW(oo_socket::udp::socket(55555, "127.0.0.1"));
}

TEST_CASE("Check receive timeout.", "[socket::udp::socket][test]") {
	// Valid port and any address
	std::shared_ptr<oo_socket::udp::socket> s1;
	char buffer[256];
	REQUIRE_NOTHROW(s1 = std::make_shared<oo_socket::udp::socket>(6666));
	REQUIRE_NOTHROW(s1->set_socket_receive_timeout(1000));
	REQUIRE(s1->receive().size() == 0);
	REQUIRE(s1->receive(reinterpret_cast<char*>(&buffer), 256) == 0);
}

TEST_CASE("Check send and receive without timeout.", "[socket::udp::socket][test]") {
	std::shared_ptr<oo_socket::udp::socket> s1;
	std::shared_ptr<oo_socket::udp::socket> s2;
	REQUIRE_NOTHROW(s1 = std::make_shared<oo_socket::udp::socket>(16666));
	REQUIRE_NOTHROW(s2 = std::make_shared<oo_socket::udp::socket>());

	SECTION("Sending data using vector and receiving using vector.") {
		std::thread send_thread = std::thread([s2]() {
			std::vector<char> buffer = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\0'};
			s2->send_to(buffer, 16666);
		});
		REQUIRE(std::string(s1->receive().data()).compare("hello world!") == 0);
		send_thread.join();
	}

	SECTION("Sending data using char* and receiving using vector.") {
		std::thread send_thread = std::thread([s2]() {
			char buffer[] = "hello world!";
			s2->send_to(reinterpret_cast<char*>(&buffer), sizeof(buffer), 16666);
		});
		REQUIRE(std::string(s1->receive().data()).compare("hello world!") == 0);
		send_thread.join();
	}

	SECTION("Sending data using vector and receiving using char*.") {
		std::thread send_thread = std::thread([s2]() {
			std::vector<char> buffer = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\0'};
			s2->send_to(buffer, 16666);
		});
		char* buffer = (char*)malloc(256);
		REQUIRE(s1->receive(buffer, 256) > 0);
		REQUIRE(std::string(buffer).compare("hello world!") == 0);
		send_thread.join();
		free(buffer);
	}

	SECTION("Sending data using char* and receiving using char*.") {
		std::thread send_thread = std::thread([s2]() {
			char buffer[] = "hello world!";
			s2->send_to(reinterpret_cast<char*>(&buffer), sizeof(buffer), 16666);
		});
		char* buffer = (char*)malloc(256);
		REQUIRE(s1->receive(buffer, 256) > 0);
		REQUIRE(std::string(buffer).compare("hello world!") == 0);
		send_thread.join();
		free(buffer);
	}
}

TEST_CASE("Check configuring the remote host.", "[socket::udp::socket][test]") {
	std::shared_ptr<oo_socket::udp::socket> s1;
	std::shared_ptr<oo_socket::udp::socket> s2;
	REQUIRE_NOTHROW(s1 = std::make_shared<oo_socket::udp::socket>(16666));
	REQUIRE_NOTHROW(s2 = std::make_shared<oo_socket::udp::socket>());

	SECTION("Configure the remote host sending data.") {
		REQUIRE_NOTHROW(s2->configure_remote_host(16666));
		std::thread send_thread = std::thread([s2]() {
			std::vector<char> buffer = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\0'};
			s2->send(buffer);
		});
		REQUIRE(std::string(s1->receive().data()).compare("hello world!") == 0);
		send_thread.join();
	}
}

TEST_CASE("Benchmarking socket.", "[socket::udp::socket][benchmark]") {
	BENCHMARK("Benchmark socket constructor/destructor with no port or address.") {
		auto socket = oo_socket::udp::socket();
		return;
	};

	BENCHMARK("Benchmark socket constructor/destructor with port.") {
		auto socket = oo_socket::udp::socket(10101);
		return;
	};

	
	auto send_socket_vector_no_remote = oo_socket::udp::socket();
	std::vector<char> send_buffer_vector(256, 'T');
	BENCHMARK("Benchmark socket send_to localhost with vector<char> of size 256.") {
		return send_socket_vector_no_remote.send_to(send_buffer_vector, 10102);
	};

	auto send_socket_vector_remote = oo_socket::udp::socket();
	send_socket_vector_remote.configure_remote_host(10103);
	BENCHMARK("Benchmark socket send localhost with vector<char> of size 256.") {
		return send_socket_vector_remote.send(send_buffer_vector);
	};

	auto send_socket_char_no_remote = oo_socket::udp::socket();
	char* send_buffer_char = (char*)malloc(256);
	memset(send_buffer_char, 'T', 256);
	BENCHMARK("Benchmark socket send_to localhost with char* of size 256.") {
		return send_socket_char_no_remote.send_to(send_buffer_char, 256, 10104);
	};

	auto send_socket_char_remote = oo_socket::udp::socket();
	send_socket_char_remote.configure_remote_host(10105);
	BENCHMARK("Benchmark socket send localhost with char* of size 256.") {
		return send_socket_char_remote.send(send_buffer_char, 256);
	};
}
