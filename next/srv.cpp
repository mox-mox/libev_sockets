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
Socket_connection::Socket_connection(const std::string& socket_path) : socket_path(socket_path), socket_watcher(loop)
{
	//{{{ Standard Unix socket creation


	int fd;
	if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 )
	{
		throw std::runtime_error("Could not create a Unix socket.");
	}

	socket_watcher.set(fd, ev::READ);
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;


	if(socket_path.length() >= sizeof(addr.sun_path)-1)
	{
		throw std::runtime_error("Unix socket path \"" + socket_path + "\" is too long. "
		                         "Maximum allowed size is " + std::to_string(sizeof(addr.sun_path)) + "." );
	}
	//std::strncpy(addr.sun_path, socket_path.c_str(), socket_path.length()+1);


	//{{{
	for(unsigned int i = 0; i<=sizeof(socket_path); i++)
	{
		addr.sun_path[i] = socket_path[i]; // Need to do this in a loop, because the usual string copying functions break when there is a '\0' character in the string.
	}
	//}}}

	unlink(&socket_path[0]);

	if( bind(fd, static_cast<struct sockaddr*>(static_cast<void*>(&addr)), socket_path.length()+1) == -1 )
	{
		throw std::runtime_error("Could not bind to socket " + socket_path + ".");
	}

	if( listen(fd, 5) == -1 )
	{
		throw std::runtime_error("Could not listen() to socket " + socket_path + ".");
	}
	//}}}

	socket_watcher.set<Socket_connection, &Socket_connection::socket_watcher_cb>(nullptr);
	socket_watcher.start();
}
//}}}

//{{{
Socket_connection::~Socket_connection(void)
{
	client_watcher* head = static_cast<client_watcher*>(socket_watcher.data);
	if(head)
	{
		client_watcher* w = head->next;
		while(w != head)
		{
			client_watcher* current = w;
			w = w->next;
			delete current;
		}
		delete head;
	}
}
//}}}

//{{{
void Socket_connection::socket_watcher_cb(ev::io& socket_watcher, int revents)
{
	std::cout<<"socket_watcher_cb called"<<std::endl;
	(void) socket_watcher;
	(void) revents;
	client_watcher* head = static_cast<client_watcher*>(socket_watcher.data);
	head = new client_watcher(socket_watcher.fd, head, socket_watcher.loop);
	head->set<Socket_connection, reinterpret_cast<void (Socket_connection::*)(ev::io& socket_watcher, int revents)>( &Socket_connection::client_watcher_cb) >(this);
	socket_watcher.data = static_cast<void*>(head);



	//}}}
}
//}}}

//{{{
bool Socket_connection::read_n(int fd, char buffer[], int size, client_watcher& watcher)	// Read exactly size bytes
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
				delete &watcher;
				return true;
			default:
				read_count+=n;
		}
	}
	return false;
}
//}}}

//{{{
void Socket_connection::client_watcher_cb(client_watcher& watcher, int revents)
{
	std::cout<<"client_watcher_cb("<<revents<<") called for fd "<<watcher.fd<<"."<<std::endl;
	//(void) revents;

	struct header_t header;

	if(read_n(watcher.fd, static_cast<char*>(static_cast<void*>(&header)), sizeof(header), watcher))
	{
				return;
	}
	std::cout<<header.doodleversion<<", length: "<<header.length<<": ";

	std::string buffer(header.length, '\0');
	if(read_n(watcher.fd, &buffer[0], header.length, watcher))
	{
				return;
	}

	std::cout<<"\""<<buffer<<"\""<<std::endl;

	for(int i=buffer.length()-1; i>=0 ; i--) // Reverse the string and send it back
	{                                        // to test the receive channel of the client
		write(watcher.fd, &buffer[i], 1);
	}

	if(buffer=="kill")
	{
		std::cout<<"Shutting down"<<std::endl;
		//todo: delete all stuff
		watcher.loop.break_loop(ev::ALL);
	}
}
//}}}


//{{{
void Socket_connection::operator()(void)
{
	std::cout<<">>>>>>>>>> Listening to connections <<<<<<<<<<"<<std::endl;
	loop.run();
	std::cout<<">>>> Not listening to connections anymore <<<<"<<std::endl;
}
//}}}





//{{{
int main()
{
	//Socket_connection connection("./This_is_a_maximum_size_socket_name_A_socket_name_with_more_chars_will_not_fit_in_the_addr_sun_path_field");
	//Socket_connection connection("./socket");
	{
	Socket_connection connection;
	connection();
	}
	std::cout<<"ASDF"<<std::endl;


	return 0;
}
//}}}


