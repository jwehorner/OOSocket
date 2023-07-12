#ifndef SOCKET_ERRORS_HPP
#define SOCKET_ERRORS_HPP

// C++ Standard Library Headers
#include <exception>
#include <map>
#include <string>
#include <vector>

namespace oo_socket {
	namespace errors {
		enum codes : uint8_t
		{
			INITIALIZATION_ERROR = 0,
			CONFIGURATION_ERROR,
			RECEIVE_ERROR,
			SEND_ERROR,
		};

		class socket_error : public std::exception {
		public:
			const char* what() const throw() {
				return message.c_str();
			}

			virtual const codes code() = 0;
		protected:
			std::string message;
		};

		class initialization_error : public socket_error {
		public:
			initialization_error(std::string additional_message = "")
			{
				message = "Could not initialize socket:\n" + additional_message;
			}
			virtual const codes code() override {return codes::INITIALIZATION_ERROR;}
		};

		class configuration_error : public socket_error {
		public:
			configuration_error(std::string additional_message = "")
			{
				message = "Could not configure socket:\n" + additional_message;
			}
			virtual const codes code() override {return codes::CONFIGURATION_ERROR;}
		};

		class receive_error : public socket_error {
		public:
			receive_error(std::string additional_message = "")
			{
				message = "Error occurred while receiving from socket:\n" + additional_message;
			}
			virtual const codes code() override {return codes::RECEIVE_ERROR;}
		};

		class send_error : public socket_error {
		public:
			send_error(std::string additional_message = "")
			{
				message = "Error occurred while sending from socket:\n" + additional_message;
			}
			virtual const codes code() override {return codes::SEND_ERROR;}
		};
	}
}

#endif /* SOCKET_ERRORS_HPP */