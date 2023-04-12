/**
 * 	@file 	UDPSocket.hpp
 * 	@brief 	Class UDPSocket is used to encapsulate the operations provided by a UDP socket into an object oriented class.
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

/// Macro for the maximum buffer when receiving data.
#define MAX_RECEIVE_BUFFER_SIZE 1500

/**
 *	@class	UDPSocket
 * 	@brief 	Class UDPSocket is used to encapsulate the operations provided by a UDP socket into an object oriented class.
 */
class UDPSocket {
public:
	UDPSocket(unsigned short port = 0, std::string address = "") : local_port(port) {
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
				throw std::runtime_error(format_message("Provided address was invalid."));
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
			int error_value_size = sizeof(error_value);
#ifdef _WIN32
			int return_code = getsockopt(socket_file_descriptor, SOL_SOCKET, SO_ERROR, (char *)&error_value, &error_value_size); 
#else
			int return_code = getsockopt(socket_file_descriptor, SOL_SOCKET, SO_ERROR, (char *)&error_value, &error_value_size); 
#endif
			throw std::runtime_error(format_message("Could not create socket, failed with error: " + std::to_string(get_last_network_error()))); 
		}

		// Set the reuse address option for the socket and allow the socket to broadcast.
		int on = 1;
		setsockopt(socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
		setsockopt(socket_file_descriptor, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on));
		
		set_socket_receive_timeout(0);

		// Fix WSA Error 10054 being thrown by recv after send to unreachable destination.
#ifdef _WIN32
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(socket_file_descriptor, _WSAIOW(IOC_VENDOR, 12), &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

		// Bind socket to local address provided earlier.
		int return_code = bind(socket_file_descriptor, (struct sockaddr *) &local_address, sizeof(struct sockaddr_in));
		if (return_code) {
			throw std::runtime_error(format_message("Could not bind socket to local address, failed with error: " + std::to_string(get_last_network_error())));
		}
	}

	/**
	 * 	@brief 	Destructor for the UDPSocket class which closes the socket.
	 */
	~UDPSocket() {
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

	/**
	 * @brief 	Method receive receives data using the socket and returns the contents as a vector of bytes.
	 * @param 	buffer_size 		size of the buffer to be allocated for the storing of incoming packets (default 1500).
	 * @param 	flags 				any flags that the packet should be received with (default 0).
	 * @return 	std::vector<char>	bytes that were received from the network, empty if the receive timed out.
	 * @throws	runtime error if an error occured while receiving the data.
	 */
	std::vector<char> receive(uint16_t buffer_size = MAX_RECEIVE_BUFFER_SIZE, int flags = 0) {
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
			if (error_code == ETIMEDOUT)
#endif
			{
				return std::vector<char>();
			}
			else {
				throw std::runtime_error(format_message("An error occured while receiving data: " + std::to_string(get_last_network_error()), "ERROR"));
				return std::vector<char>();
			}
		}
		// Else, preallocate a vector based on the number of bytes actually received, copy the contents, then return it.
		else {
			std::vector<char> data = std::vector<char>(receive_size);
			memcpy(data.data(), buffer, receive_size);
			return data;
		}
	}

	/**
	 * @brief 	Method sendTo sends a string buffer of bytes to a specified remote host. 
	 * @param 	buffer	vector of bytes to send to the remote host.
	 * @param 	port 	unsigned short port number to send the packet to.
	 * @param 	address string representation of the address of the remote host to send the packet to (default loopback).
	 * @param 	flags 	any flags that the packet should be sent with (default 0).
	 * @return 	int 	number of bytes sent.
 	 * @throws	runtime error if the address of the remote host is invalid or if an error occured while sending the data.
	 */
	int send_to(std::vector<char> buffer, unsigned short port, std::string address = "127.0.0.1", int flags = 0) {
		// Lock the mutex so the socket to prevent race conditions.
		std::unique_lock<std::mutex> send_lock(send_mutex);

		// Populate a temporary struct to hold the destination address.
		sockaddr_in address_struct;
		address_struct.sin_family = AF_INET;
		address_struct.sin_port = htons(port);
    	if (inet_pton(AF_INET, address.c_str(), &address_struct.sin_addr) != 1) {
			throw std::runtime_error(format_message("Provided address was invalid."));
		}
		
#ifdef _WIN32
		size_t result;
#else
		int result;
#endif
		// Send the contents of the string buffer using sendto.
		result = sendto(socket_file_descriptor, buffer.data(), buffer.size(), flags, (const struct sockaddr *)&address_struct, sizeof(address_struct));
		
		// If an error occurs, throw an error.
		if (result == -1) {
			throw std::runtime_error(format_message("An error occured while sending data: " + std::to_string(get_last_network_error()), "ERROR"));
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
	 * @throws	runtime error if the remote host has not been pre-configured or if an error occured while sending the data.
	 */
	int send(std::vector<char> buffer, int flags = 0) {
		// Lock the mutex so the socket to prevent race conditions.
		std::unique_lock<std::mutex> access_lock(member_mutex);
		std::unique_lock<std::mutex> send_lock(send_mutex);

		if (remote_address_set) {
#ifdef _WIN32
			size_t result;
#else
			int result;
#endif
			// Send the contents of the string buffer to the preconfigured remote host.
			result = sendto(socket_file_descriptor, buffer.data(), buffer.size(), flags, (const struct sockaddr *)&remote_address, sizeof(remote_address));
			
			// If an error occurs, throw an error.
			if (result == -1) {
				throw std::runtime_error(format_message("An error occured while sending data: " + std::to_string(get_last_network_error()), "ERROR"));
				return -1;
			}
			// Else return the number of bytes sent.
			else {
				return result;
			}
		}
		else {
			throw std::runtime_error(format_message("Remote host address and port has not been set.", "WARNING"));
			return -1;
		}
	}

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
			throw std::runtime_error(format_message("Provided address was invalid."));
		}

		remote_address_set = true;
	}

	void set_socket_receive_timeout(unsigned int timeout_ms) {
#ifdef _WIN32
		// windows wants timeouts as DWORD in ms
		DWORD timeout_ms_long = (DWORD)(timeout_ms);
		if (setsockopt(socket_file_descriptor, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms_long, sizeof(timeout_ms_long))) {
			throw std::runtime_error(format_message("An error occured while setting the receive timeout: " + std::to_string(get_last_network_error()), "ERROR"));
		}
#else
		struct timeval timeout_struct;
		// Get the equivalent number of seconds from the milliseconds.
		timeout_struct.tv_sec = (int)(timeout_ms / 1000.0);
		// Set the remaining number of microseconds.
		// Remaining microseconds = ((total milliseconds) - (seconds * 1000)) * 1000
		timeout_struct.tv_usec = (int)(timeout_ms - (timeout_struct.tv_sec * 1000.0)) * 1000;
		if (setsockopt(sock_data->s, SOL_SOCKET, SO_RCVTIMEO, &timeout_struct, sizeof(timeout_struct))) {
			throw std::runtime_error(format_message("An error occured while setting the receive timeout: " + std::to_string(get_last_network_error()), "ERROR"));
		}
#endif   
	}

protected:
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

	/**
	 * 	@brief	Method initialize_windows_sockets starts WSA in preparation for using sockets in Windows.
	 * 	@throws	runtime error if WSA fails to start.
	*/
	void initialize_windows_sockets(void) {
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;
		/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
		wVersionRequested = MAKEWORD(2, 2);	// We'll ask for ver 2.2
		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0) {
			/* Tell the user that we could not find a usable */
			/* Winsock DLL.                                  */
			throw std::runtime_error(format_message("WSAStartup failed with error: " + WSAGetLastError()));
		}
	}

	/**
	 *	@brief	Method format_message is used to return a formatted string with a message and severity level.
	 *	@param	message	string message to include in the string.
	 *	@param	level	string level of the message indicating the severity (default ERROR).
	 *	@return string	formatted string with message and severity level.
	*/
	std::string format_message(std::string message, std::string level = "ERROR") {
		return std::string("[UDPSocket] (" + level + ") " + message + "\n");
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

#endif /* UDP_SOCKET_HPP */