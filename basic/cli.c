// compile with g++ timer.cpp -lev
#include <sys/socket.h>
#include <sys/un.h>
#include <ev++.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

const std::string socket_path = "./socket";



int main(void)
{
	//{{{ Standard Unix socket creation

	int socket_fd;
	if((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 )
	{
		throw std::runtime_error("Could not create a Unix socket.");
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	if( socket_path.length() >= sizeof(addr.sun_path)-1 )
	{
		throw std::runtime_error("Unix socket path \""+socket_path+"\" is too long. "
			"Maximum allowed size is "+std::to_string (sizeof(addr.sun_path))+".");
	}
	std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

	if( connect(socket_fd, (struct sockaddr*) &addr, sizeof(addr)) == -1 )
	{
		throw std::runtime_error("Could not connect to socket "+socket_path+".");
	}

	//}}}


	std::string buf;

	while( true )
	{
		std::cin>>buf;
		int rc;
		if((rc = write(socket_fd, &buf[0], buf.length()) != static_cast<int>(buf.length())))
		{
			if( rc > 0 )
			{
				std::cerr<<"Partial write only"<<std::endl;
			}
			else
			{
				std::cerr<<"Write error"<<std::endl;
				return -1;
			}
		}
		buf.clear();
	}

	return 0;
}
