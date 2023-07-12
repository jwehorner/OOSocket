/**
 * 	@file 	socket.hpp
 * 	@brief 	Class socket is used to encapsulate the operations provided by a UDP socket into an object oriented class.
 * 	@author James Horner
 * 	@date 	2023-04-05
 */

#ifndef UDP_SOCKET_HPP
#define UDP_SOCKET_HPP

// Standard System Libraries
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// Platform Specific System Libraries
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "errors.hpp"

/// Macro for the maximum buffer when receiving data.
#define MAX_RECEIVE_BUFFER_SIZE 1500

namespace oo_socket
{
	namespace udp
	{
		/**
		 *	@class	socket
		* 	@brief 	Class socket is used to encapsulate the operations provided by a UDP socket into an object oriented class.
		*/
		class socket {
		public:
			/**************************************************************************************************/
			/* Non-Static Methods			 																  */
			/**************************************************************************************************/

			/**
			 * @brief 	Constructor for the socket class.
			 * @details	Constructor initialises the socket descriptors and performs configuration on the socket
			 * 			before binding.
			 * @param 	port 	unsigned short port number to bind the socket to (default 0).
			 * @param 	address string address to bind the socket to (default "").
			 */
			socket(unsigned short port = 0, std::string address = "") : local_port(port) {
				// Before doing anything make sure winsock is started.
#ifdef _WIN32
				initialize_windows_sockets();
#endif
				// Initialize the remote address flag to false.
				remote_address_set = false;

				// Specify address family of the socket.
				local_address.sin_family = AF_INET;
				
				// Set the port of the socket.
				local_address.sin_port = htons(port);

				// If no address is provided, just set the local address as any,
				if (address.compare("") == 0) {
					local_address.sin_addr.s_addr = INADDR_ANY;
				}
				// If an address is provided, try to parse the string into a network representation.
				else {
					if (inet_pton(AF_INET, address.c_str(), (void *)&local_address.sin_addr.s_addr) != 1) {
						throw errors::initialization_error("Provided address was invalid.");
					}
				}

				// Get a socket file descriptor and store it in the class member.
#ifdef _WIN32
				socket_file_descriptor = WSASocketA(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
				socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
#endif

				// If there is an error getting the socket descriptor, throw an error.
				if(socket_file_descriptor < 0 ) {
					int error_value;
					unsigned int error_value_size = sizeof(error_value);
#ifdef _WIN32
					int return_code = getsockopt(socket_file_descriptor, SOL_SOCKET, SO_ERROR, (char *)&error_value, (int*)&error_value_size); 
#else
					int return_code = getsockopt(socket_file_descriptor, SOL_SOCKET, SO_ERROR, (char *)&error_value, &error_value_size); 
#endif
					throw errors::initialization_error("Could not create socket, failed with error: " + std::to_string(get_last_network_error()));
				}

				// Set the reuse address option for the socket and allow the socket to broadcast.
				int on = 1;
				setsockopt(socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
				setsockopt(socket_file_descriptor, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on));
				
				set_socket_receive_timeout(0);

				// Fix WSA Error 10054 being thrown by recv after send to unreachable destination.
#ifdef _WIN32
				BOOL new_behavior = FALSE;
				DWORD bytes_returned = 0;
				WSAIoctl(socket_file_descriptor, _WSAIOW(IOC_VENDOR, 12), &new_behavior, sizeof new_behavior, NULL, 0, &bytes_returned, NULL, NULL);
#endif

				// Bind socket to local address provided earlier.
				int return_code = bind(socket_file_descriptor, (struct sockaddr *) &local_address, sizeof(struct sockaddr_in));
				if (return_code) {
					throw errors::initialization_error("Could not bind socket to local address, failed with error: " + std::to_string(get_last_network_error()));
				}
			}

			/**
			 * 	@brief 	Destructor for the socket class which closes the socket.
			 */
			~socket() {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> access_lock(member_mutex);
				std::unique_lock<std::mutex> send_lock(send_mutex);

				// Close the socket file descriptor.
#ifdef _WIN32
				closesocket(socket_file_descriptor);
#else
				close(socket_file_descriptor);
#endif
			}

			/**************************************************************************************************/
			/* Send/Receive Methods			 																  */
			/**************************************************************************************************/
			/**
			 * @brief 	Method receive receives data using the socket and returns the contents as a vector of bytes.
			 * @param 	buffer_size 		size of the buffer to be allocated for the storing of incoming packets (default 1500).
			 * @param 	flags 				any flags that the packet should be received with (default 0).
			 * @return 	std::vector<T>		bytes that were received from the network, empty if the receive timed out.
			 * @throws	receive_error if an error occurred while receiving the data.
			 */
			template <typename T = char>
			std::vector<T> receive(const uint16_t buffer_size = MAX_RECEIVE_BUFFER_SIZE, const int flags = 0) {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> receive_lock(receive_mutex);

				// Allocate the receive buffer based on the estimate provided.
				char *buffer = (char*)malloc(buffer_size);
				memset(buffer, 0, buffer_size);

				// Receive the packet.
				int receive_size = recv(socket_file_descriptor, buffer, buffer_size, flags);

				// If an error occurs, throw an error.
				if (receive_size == -1) {
					int error_code = get_last_network_error();
#ifdef _WIN32
					if (error_code == WSAETIMEDOUT)
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK)
#endif
					{
						free(buffer);
						return std::vector<T>();
					}
					else {
						free(buffer);
						throw errors::receive_error(std::to_string(error_code));
						return std::vector<T>();
					}
				}
				// Else, preallocate a vector based on the number of bytes actually received, copy the contents, then return it.
				else {
					std::vector<T> data = std::vector<T>(std::ceil(receive_size / sizeof(T)));
					memcpy(data.data(), buffer, receive_size);
					free(buffer);
					return data;
				}
			}

			/**
			 * @brief 	Method receive receives data using the socket and returns the contents as a vector of bytes.
			 * @param 	buffer[out] 		buffer that will store the incoming packet.
			 * @param 	buffer_size[in]		size of the buffer to be allocated for the storing of incoming packets (default 1500).
			 * @param 	flags[in]			any flags that the packet should be received with (default 0).
			 * @return 	int					number of bytes that were received from the network, 0 if the receive timed out.
			 * @throws	receive_error if an error occurred while receiving the data.
			 */
			int receive(char* buffer, const uint16_t buffer_size = MAX_RECEIVE_BUFFER_SIZE, const int flags = 0) {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> receive_lock(receive_mutex);

				// Receive the packet.
				int receive_size = recv(socket_file_descriptor, buffer, buffer_size, flags);

				// If an error occurs, throw an error.
				if (receive_size == -1) {
					int error_code = get_last_network_error();
#ifdef _WIN32
					if (error_code == WSAETIMEDOUT)
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK)
#endif
					{
						return 0;
					}
					else {
						throw errors::receive_error(std::to_string(error_code));
					}
				}
				// Else, preallocate a vector based on the number of bytes actually received, copy the contents, then return it.
				else {
					return receive_size;
				}
			}

			/**
			 * @brief 	Method sendTo sends a string buffer of bytes to a specified remote host. 
			 * @param 	buffer	vector of bytes to send to the remote host.
			 * @param 	port 	unsigned short port number to send the packet to.
			 * @param 	address string representation of the address of the remote host to send the packet to (default loopback).
			 * @param 	flags 	any flags that the packet should be sent with (default 0).
			 * @return 	int 	number of bytes sent.
			 * @throws	send_error if the address of the remote host is invalid or if an error occurred while sending the data.
			 */
			template <typename T>
			int send_to(const std::vector<T> buffer, const unsigned short port, const std::string address = "127.0.0.1", const int flags = 0) {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> send_lock(send_mutex);

				// Populate a temporary struct to hold the destination address.
				sockaddr_in address_struct;
				address_struct.sin_family = AF_INET;
				address_struct.sin_port = htons(port);
				if (inet_pton(AF_INET, address.c_str(), &address_struct.sin_addr) != 1) {
					throw errors::send_error("Provided address was invalid.");
				}
				
				// Send the contents of the string buffer using sendto.
				int result = sendto(socket_file_descriptor, reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(T), flags, (const struct sockaddr *)&address_struct, sizeof(address_struct));
				
				// If an error occurs, throw an error.
				if (result == -1) {
					throw errors::send_error(std::to_string(get_last_network_error()));
					return -1;
				}
				// Else return the number of bytes sent.
				else {
					return result;
				}

			}

			/**
			 * @brief 	Method send is used to send a packet to a remote host pre-configured using configure_remote_host.
			 * @param 	buffer	string of bytes to send to the remote host.
			 * @param 	flags 	any flags that the packet should be sent with (default 0).
			 * @return 	int 	number of bytes sent.
			 * @throws	send_error if the remote host has not been pre-configured or if an error occurred while sending the data.
			 */
			template <typename T>
			int send(const std::vector<T> buffer, const int flags = 0) {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> access_lock(member_mutex);
				std::unique_lock<std::mutex> send_lock(send_mutex);

				if (remote_address_set) {
					// Send the contents of the string buffer to the preconfigured remote host.
					int result = sendto(socket_file_descriptor, reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(T), flags, (const struct sockaddr *)&remote_address, sizeof(remote_address));
					
					// If an error occurs, throw an error.
					if (result == -1) {
						throw errors::send_error(std::to_string(get_last_network_error()));
						return -1;
					}
					// Else return the number of bytes sent.
					else {
						return result;
					}
				}
				else {
					throw errors::send_error("Remote host address and port has not been set.");
					return -1;
				}
			}

			/**
			 * @brief 	Method sendTo sends a string buffer of bytes to a specified remote host. 
			 * @param 	buffer		pointer to buffer of bytes to send to the remote host.
			 * @param 	buffer_size	size of buffer in bytes.
			 * @param 	port 		unsigned short port number to send the packet to.
			 * @param 	address 	string representation of the address of the remote host to send the packet to (default loopback).
			 * @param 	flags 		any flags that the packet should be sent with (default 0).
			 * @return 	int 		number of bytes sent.
			 * @throws	send_error if the address of the remote host is invalid or if an error occurred while sending the data.
			 */
			int send_to(const char* buffer, const size_t buffer_size, const unsigned short port, const std::string address = "127.0.0.1", const int flags = 0) {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> send_lock(send_mutex);

				// Populate a temporary struct to hold the destination address.
				sockaddr_in address_struct;
				address_struct.sin_family = AF_INET;
				address_struct.sin_port = htons(port);
				if (inet_pton(AF_INET, address.c_str(), &address_struct.sin_addr) != 1) {
					throw errors::send_error("Provided address was invalid.");
				}
				
				// Send the contents of the string buffer using sendto.
				int result = sendto(socket_file_descriptor, buffer, buffer_size, flags, (const struct sockaddr *)&address_struct, sizeof(address_struct));
				
				// If an error occurs, throw an error.
				if (result == -1) {
					throw errors::send_error(std::to_string(get_last_network_error()));
					return -1;
				}
				// Else return the number of bytes sent.
				else {
					return result;
				}

			}

			/**
			 * @brief 	Method send is used to send a packet to a remote host pre-configured using configure_remote_host.
			 * @param 	buffer	string of bytes to send to the remote host.
			 * @param 	flags 	any flags that the packet should be sent with (default 0).
			 * @return 	int 	number of bytes sent.
			 * @throws	send_error if the remote host has not been pre-configured or if an error occurred while sending the data.
			 */
			int send(const char* buffer, const size_t buffer_size, const int flags = 0) {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> access_lock(member_mutex);
				std::unique_lock<std::mutex> send_lock(send_mutex);

				if (remote_address_set) {
					// Send the contents of the string buffer to the preconfigured remote host.
					int result = sendto(socket_file_descriptor, buffer, buffer_size, flags, (const struct sockaddr *)&remote_address, sizeof(remote_address));
					
					// If an error occurs, throw an error.
					if (result == -1) {
						throw errors::send_error(std::to_string(get_last_network_error()));
						return -1;
					}
					// Else return the number of bytes sent.
					else {
						return result;
					}
				}
				else {
					throw errors::send_error("Remote host address and port has not been set.");
					return -1;
				}
			}


			/**************************************************************************************************/
			/* Configuration Methods		 																  */
			/**************************************************************************************************/
			/**
			 * @brief 	Method getSocketFileDescriptor returns the socket file descriptor for use in additional lower level configuration.
			 * @return 	unsigned long long file descriptor of the socket.
			 */
			unsigned long long get_socket_file_descriptor() {
				return socket_file_descriptor;
			}

			/**
			 * @brief 	Method configure_remote_address is used to configure a remote host to which packets
			 * 			can be sent without having to provide the destination each time.
			 * @param 	port 	unsigned short port number of the remote host.
			 * @param 	address	string representation of the address of the remote host (default loopback).
			 * @throws	configuration_error if the provided address is invalid.
			 */
			void configure_remote_host(unsigned short port, std::string address = "127.0.0.1") {
				// Lock the mutex so the socket to prevent race conditions.
				std::unique_lock<std::mutex> access_lock(member_mutex);

				// Specify address family of the destination.
				remote_address.sin_family = AF_INET;
				
				// Set the port of the socket.
				remote_address.sin_port = htons(port);

				// If an address is provided, try to parse the string into a network representation.
				if (inet_pton(AF_INET, address.c_str(), (void *)&remote_address.sin_addr.s_addr) != 1) {
					throw errors::configuration_error("Provided address was invalid.");
				}

				remote_address_set = true;
			}

			/**
			 * @brief 	Method set_socket_receive_timeout is used to configure the socket to time out on 
			 * 			receive calls after the specified number of milliseconds.
			 * @param 	timeout_ms 	unsigned int number of milliseconds before calls to receive time out.
			 * @throws	configuration_error if the timeout could not be set. 
			 */
			void set_socket_receive_timeout(unsigned int timeout_ms) {
#ifdef _WIN32
				// windows wants timeouts as DWORD in ms
				DWORD timeout_ms_long = (DWORD)(timeout_ms);
				if (setsockopt(socket_file_descriptor, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms_long, sizeof(timeout_ms_long))) {
					throw errors::configuration_error("An error occurred while setting the receive timeout: " + std::to_string(get_last_network_error()));
				}
#else
				struct timeval timeout_struct;
				// Get the equivalent number of seconds from the milliseconds.
				timeout_struct.tv_sec = (int)(timeout_ms / 1000.0);
				// Set the remaining number of microseconds.
				// Remaining microseconds = ((total milliseconds) - (seconds * 1000)) * 1000
				timeout_struct.tv_usec = (int)(timeout_ms - (timeout_struct.tv_sec * 1000.0)) * 1000;
				if (setsockopt(socket_file_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout_struct, sizeof(timeout_struct))) {
					throw errors::configuration_error("An error occurred while setting the receive timeout: " + std::to_string(get_last_network_error()));
				}
#endif   
			}

		protected:
			/**************************************************************************************************/
			/* Non-Static Members			 																  */
			/**************************************************************************************************/
			/// File descriptor of the socket that the class uses.
			unsigned long long socket_file_descriptor;

			/// The local port number of the socket. 
			unsigned short local_port;
			/// Struct holding the local address of the socket. 
			sockaddr_in local_address;

			/// Struct holding the preconfigured remote address of the destination. 
			sockaddr_in remote_address;
			/// Flag for if the remote address has been preconfigured.  
			bool remote_address_set;

			/// Mutex to control ability to access private variables in the socket.
			std::mutex member_mutex;

			/// Mutex to control ability to send using the socket.
			std::mutex send_mutex;

			/// Mutex to control ability to receive using the socket.
			std::mutex receive_mutex;


			/**************************************************************************************************/
			/* Non-Static Methods			 																  */
			/**************************************************************************************************/
			/**
			 * 	@brief	Method initialize_windows_sockets starts WSA in preparation for using sockets in Windows.
			 * 	@throws	initialization_error if WSA fails to start.
			*/
			void initialize_windows_sockets(void) {
#ifdef _WIN32
				WORD wVersionRequested;
				WSADATA wsaData;
				int err;
				/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
				wVersionRequested = MAKEWORD(2, 2);	// We'll ask for ver 2.2
				err = WSAStartup(wVersionRequested, &wsaData);
				if (err != 0) {
					/* Tell the user that we could not find a usable */
					/* Winsock DLL.                                  */
					throw errors::initialization_error("WSAStartup failed with error: " + WSAGetLastError());
				}
#endif
			}

			/**
			 *	@brief	Method get_last_network_error retrieves the last networking error.
			*	@return	int value from WSA or errno.
			*/
			int get_last_network_error() {
#ifdef _WIN32
				return WSAGetLastError();
#else
				return errno;
#endif
			}
		};
	}
}

#endif /* UDP_SOCKET_HPP */