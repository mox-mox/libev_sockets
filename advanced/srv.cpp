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


//{{{
void Socket_connection::client_watcher_cb(ev::socket& socket_watcher, int revents)
{
	std::cout<<"client_watcher_cb called for fd "<<socket_watcher.fd<<"."<<std::endl;
	(void) revents;


	std::string buffer(RECEIVE_BUFFER_SIZE+1, '\0');
	int n;
	switch((n=read(socket_watcher.fd, &buffer[0], RECEIVE_BUFFER_SIZE)))
	{
		case -1:
			throw std::runtime_error("Read error on the connection using fd." + std::to_string(socket_watcher.fd) + ".");
		case  0:
			std::cout<<"Received EOF (Client has closed the connection)."<<std::endl;
			close(socket_watcher.fd);
			socket_watcher.stop();
			delete &socket_watcher;
			return;
		default:
			buffer.resize(n);
			std::cout<<"Received: |"<<buffer<<"|"<<std::endl;
			if(buffer=="kill")
			{
				std::cout<<"Shutting down"<<std::endl;
				close(socket_watcher.fd);
				unlink(socket_path.c_str());
				loop.break_loop(ev::ALL);
			}

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
	Socket_connection connection;
	connection();


	return 0;
}
//}}}


