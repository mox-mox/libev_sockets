#include "srv.hpp"
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>

#define RECEIVE_BUFFER_SIZE 100

//{{{
Socket_connection::Socket_connection(const std::string& socket_path) : socket_watcher(loop)
{
	//{{{ Standard Unix socket creation

	int fd;
	if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 )
	{
		throw std::runtime_error("Could not create a Unix socket.");
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	if(socket_path.length() >= sizeof(addr.sun_path)-1)
	{
		throw std::runtime_error("Unix socket path \"" + socket_path + "\" is too long. "
		                         "Maximum allowed size is " + std::to_string(sizeof(addr.sun_path)) + "." );
	}
	std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);


	unlink(socket_path.c_str());

	if( bind(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1 )
	{
		throw std::runtime_error("Could not bind to socket " + socket_path + ".");
	}

	if( listen(fd, 5) == -1 )
	{
		throw std::runtime_error("Could not listen() to socket " + socket_path + ".");
	}
	//}}}

	////{{{ Create a libev io watcher to respond to incomming connections

	socket_watcher.set < Socket_connection, &Socket_connection::socket_watcher_cb > (this);
	socket_watcher.set(fd, ev::READ);
	socket_watcher.start();
	////}}}
}
//}}}


//{{{
void Socket_connection::socket_watcher_cb(ev::io& socket_watcher, int revents)
{
	std::cout<<"socket_watcher_cb called"<<std::endl;
	(void) socket_watcher;
	(void) revents;

	int cl;
	if((cl = accept(socket_watcher.fd, NULL, NULL)) == -1 )
	{
		std::cerr<<"Could not accept() a connection."<<std::endl;
		return;
	}

	//{{{ Watcher for i3 events

	ev::io* client_watcher = new ev::io(loop);
	client_watcher->set<Socket_connection, &Socket_connection::client_watcher_cb>(this);
	client_watcher->set(cl, ev::READ);
	client_watcher->start();
	//}}}
}
//}}}

//{{{
void Socket_connection::client_watcher_cb(ev::io& socket_watcher, int revents)
{
	(void) revents;

	std::string buf(RECEIVE_BUFFER_SIZE, 0);
	switch(read(socket_watcher.fd, &buf[0], RECEIVE_BUFFER_SIZE))
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
		std::cout<<"Received: \""<<buf<<"\"."<<std::endl;
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


