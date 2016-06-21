#include "srv.hpp"
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <functional>

#define RECEIVE_BUFFER_SIZE 10

//{{{
Socket_connection::Socket_connection(const std::string& socket_path) : socket_path(socket_path), socket_watcher(socket(AF_UNIX, SOCK_STREAM, 0), ev::READ, loop)
{
	//{{{ Standard Unix socket creation

	socket_watcher.set<Socket_connection, &Socket_connection::socket_watcher_cb>(this);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	if(socket_path.length() >= sizeof(addr.sun_path)-1)
	{
		throw std::runtime_error("Unix socket path \"" + socket_path + "\" is too long. "
		                         "Maximum allowed size is " + std::to_string(sizeof(addr.sun_path)) + "." );
	}
	std::strncpy(addr.sun_path, socket_path.c_str(), socket_path.length()+1);

	unlink(socket_path.c_str());

	if( bind(socket_watcher.fd, (struct sockaddr*) &addr, sizeof(addr)) == -1 )
	{
		throw std::runtime_error("Could not bind to socket " + socket_path + ".");
	}

	if( listen(socket_watcher.fd, 5) == -1 )
	{
		throw std::runtime_error("Could not listen() to socket " + socket_path + ".");
	}
	//}}}

	socket_watcher.start();
}
//}}}

//{{{
void Socket_connection::socket_watcher_cb(ev::socket& socket_watcher, int revents)
{
	std::cout<<"socket_watcher_cb called"<<std::endl;
	(void) socket_watcher;
	(void) revents;

	//{{{ Watcher for i3 events

	ev::socket* client_watcher = new ev::socket(accept(socket_watcher.fd, NULL, NULL), ev::READ, loop);
	client_watcher->set<Socket_connection, &Socket_connection::client_watcher_cb>(this);




	client_watcher->start();
	//}}}
}
//}}}

bool Socket_connection::read_n(int fd, char buffer[], int size)	// Read exactly size bytes
{
	int read_count = 0;
	while(read_count < size)
	{
		int n;
		switch((n=read(fd, &buffer[read_count], size-read_count)))
		{
			case -1:
				throw std::runtime_error("Read error on the connection using fd." + std::to_string(socket_watcher.fd) + ".");
			case  0:
				std::cout<<"Received EOF (Client has closed the connection)."<<std::endl;
				return true;
			default:
				read_count+=n;
		}
	}
	return false;
}

//{{{
void Socket_connection::client_watcher_cb(ev::socket& client_watcher, int revents)
{
	std::cout<<"client_watcher_cb called for fd "<<client_watcher.fd<<"."<<std::endl;
	(void) revents;

	struct header_t header;

	if(read_n(client_watcher.fd, static_cast<char*>(static_cast<void*>(&header)), sizeof(header)))
	{
				close(client_watcher.fd);
				client_watcher.stop();
				delete &client_watcher;
	}
	std::cout<<header.doodleversion<<", length: "<<header.length<<std::endl;

	std::string buffer(header.length, '\0');
	if(read_n(client_watcher.fd, &buffer[0], header.length))
	{
				close(client_watcher.fd);
				client_watcher.stop();
				delete &client_watcher;
	}
	for(unsigned int i=0; i<buffer.length(); i++)
	{
		std::cout<<"|"<<static_cast<unsigned int>(buffer[i]);
	}
	std::cout<<"|"<<std::endl;
	std::cout<<"Received: |"<<buffer<<"|"<<std::endl;
	if(buffer=="kill")
	{
		std::cout<<"Shutting down"<<std::endl;
		close(client_watcher.fd);
		unlink(socket_path.c_str());
		loop.break_loop(ev::ALL);
	}

	//}
}
//}}}


//{{{
void Socket_connection::operator()(void)
{
	std::cout<<">>>>>>>>>> Listening to connections <<<<<<<<<<"<<std::endl;
	loop.run();
}
//}}}





//{{{
int main()
{
	//Socket_connection connection("./This_is_a_maximum_size_socket_name_A_socket_name_with_more_chars_will_not_fit_in_the_addr_sun_path_field");
	Socket_connection connection;
	connection();


	return 0;
}
//}}}

