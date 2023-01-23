#ifndef UDP_SOCKET_HPP
#define UDP_SOCKET_HPP

#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

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

#define MAX_RECEIVE_BUFFER_SIZE 1500

class UDPSocket {
public:
	UDPSocket(unsigned short port, std::string address = "") {
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
				throw_runtime_error("Provided address was invalid.");
			}
		}

		// Get a socket file descriptor and store it in the class member.
		socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0);

		// If there is an error getting the socket descriptor, throw an error.
		if(socket_file_descriptor < 0 ) {
			int error_value;
			int error_value_size = sizeof(error_value);
#ifdef _WIN32
			int return_code = getsockopt(socket_file_descriptor, SOL_SOCKET, SO_ERROR, (char *)&error_value, &error_value_size); 
#else
			int return_code = getsockopt(socket_file_descriptor, SOL_SOCKET, SO_ERROR, (char *)&error_value, &error_value_size); 
#endif
			throw_runtime_error("Could not create socket, failed with error: " + std::to_string(get_last_network_error())); 
		}

		// Set the reuse address option for the socket and allow the socket to broadcast.
		int on = 1;
		setsockopt(socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
		setsockopt(socket_file_descriptor, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on));
		
		// Fix WSA Error 10054 being thrown by recv after send to unreachable destination.
#ifdef _WIN32
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(socket_file_descriptor, _WSAIOW(IOC_VENDOR, 12), &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

		// Bind socket to local address provided earlier.
		int return_code = bind(socket_file_descriptor, (struct sockaddr *) &local_address, sizeof(struct sockaddr_in));
		if (return_code) {
			throw_runtime_error("Could not bind socket to local address, failed with error: " + std::to_string(get_last_network_error()));
		}
	}

	/**
	 * @brief 	Method receive receives data using the socket and returns the contents as a vector of bytes.
	 * @param 	buffer_size 		size of the buffer to be allocated for the storing of incoming packets (default 1500).
	 * @param 	flags 				any flags that the packet should be received with (default 0).
	 * @return 	std::vector<char>	bytes that were received from the network.
	 * @throws	runtime error if an error occured while receiving the data.
	 */
	std::vector<char> receive(uint16_t buffer_size = MAX_RECEIVE_BUFFER_SIZE, int flags = 0) {
		// Lock the mutex so the socket to prevent race conditions.
		std::unique_lock<std::mutex> receive_lock(receive_mutex);

		// Allocate the receive buffer based on the estimate provided.
		char *buffer = (char*)malloc(buffer_size);

		// Receive the packet.
		int receive_size = recv(socket_file_descriptor, buffer, buffer_size, flags);

		// If an error occurs, throw an error.
		if (receive_size == -1) {
			throw_runtime_error("An error occured while receiving data: " + std::to_string(get_last_network_error()), "ERROR");
			return std::vector<char>();
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
			throw_runtime_error("Provided address was invalid.");
		}
		
		// Send the contents of the string buffer using sendto.
		int result = sendto(socket_file_descriptor, buffer.data(), buffer.size(), flags, (const struct sockaddr *)&address_struct, sizeof(address_struct));
		
		// If an error occurs, throw an error.
		if (result == -1) {
			throw_runtime_error("An error occured while sending data: " + std::to_string(get_last_network_error()), "ERROR");
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
			// Send the contents of the string buffer to the preconfigured remote host.
			int result = sendto(socket_file_descriptor, buffer.data(), buffer.size(), flags, (const struct sockaddr *)&remote_address, sizeof(remote_address));
			
			// If an error occurs, throw an error.
			if (result == -1) {
				throw_runtime_error("An error occured while sending data: " + std::to_string(get_last_network_error()), "ERROR");
				return -1;
			}
			// Else return the number of bytes sent.
			else {
				return result;
			}
		}
		else {
			throw_runtime_error("Remote host address and port has not been set.", "WARNING");
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
			throw_runtime_error("Provided address was invalid.");
		}

		remote_address_set = true;
	}

protected:
	/// File descriptor of the socket that the class uses.
	unsigned long long socket_file_descriptor;

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
			throw_runtime_error("WSAStartup failed with error: " + WSAGetLastError());
		}
	}

	/**
	 *	@brief	Method throw_runtime_error is used to throw a runtime error with a formatted string.
	 *	@param	message	string message to include in the error.
	 *	@param	level	string level of the error indicating the severity (default ERROR).
	 *	@throws runtime error.
	*/
	void throw_runtime_error(std::string message, std::string level = "ERROR") {
		throw std::runtime_error("[UDPSocket] (" + level + ") " + message + "\n");
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