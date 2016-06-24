#include "srv.hpp"
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <functional>
#include <cstdio>

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


	//{{{
	for(unsigned int i = 0; i<=sizeof(socket_path); i++)
	{
		addr.sun_path[i] = socket_path[i]; // Need to do this in a loop, because the usual string copying functions break when there is a '\0' character in the string.
	}
	//}}}

	unlink(&socket_path[0]);

	if( bind(socket_watcher.fd, static_cast<struct sockaddr*>(static_cast<void*>(&addr)), socket_path.length()+1) == -1 )
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

	//client_watchers.push_front({accept(socket_watcher.fd, NULL, NULL), ev::READ, loop});
	//ev::socket& client_watcher = client_watchers.front();
	//client_watcher.set<Socket_connection, &Socket_connection::client_watcher_cb>(this);
	//client_watcher.start();


	ev::socket* client_watcher = new ev::socket(accept(socket_watcher.fd, NULL, NULL), ev::READ, loop);
	client_watchers.push_front(client_watcher);
	client_watcher->set<Socket_connection, &Socket_connection::client_watcher_cb>(this);
	client_watcher->start();
	//}}}
}
//}}}

//{{{
bool Socket_connection::read_n(int fd, char buffer[], int size, ev::socket& client_watcher)	// Read exactly size bytes
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
				close(client_watcher.fd);
				client_watcher.stop();
				client_watchers.remove(&client_watcher);
				delete &client_watcher;
				return true;
			default:
				read_count+=n;
		}
	}
	return false;
}
//}}}

//{{{
void Socket_connection::client_watcher_cb(ev::socket& client_watcher, int revents)
{
	std::cout<<"client_watcher_cb("<<revents<<") called for fd "<<client_watcher.fd<<"."<<std::endl;
	//(void) revents;

	struct header_t header;

	if(read_n(client_watcher.fd, static_cast<char*>(static_cast<void*>(&header)), sizeof(header), client_watcher))
	{
				return;
	}
	std::cout<<header.doodleversion<<", length: "<<header.length<<": ";

	std::string buffer(header.length, '\0');
	if(read_n(client_watcher.fd, &buffer[0], header.length, client_watcher))
	{
				return;
	}

	std::cout<<"\""<<buffer<<"\""<<std::endl;

	for(int i=buffer.length()-1; i>=0 ; i--) // Reverse the string and send it back
	{                                        // to test the receive channel of the client
		write(client_watcher.fd, &buffer[i], 1);
	}

	if(buffer=="kill")
	{
		std::cout<<"Shutting down"<<std::endl;
		for(auto watcher : client_watchers)
		{
			close(watcher->fd);
			delete watcher;

		}
		client_watchers.clear();
		unlink(&socket_path[0]);
		loop.break_loop(ev::ALL);
	}
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
	//Socket_connection connection("./socket");
	Socket_connection connection;
	connection();


	return 0;
}
//}}}


