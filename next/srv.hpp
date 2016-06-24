// compile with g++ timer.cpp -lev
#pragma once
#include <ev++.h>
#include <iostream>
#include <list>
#include "socketpath.hpp"
#include <sys/socket.h>
#include <unistd.h>




class Socket_connection
{

	//{{{
	struct client_watcher : ev::io
	{
		client_watcher(int main_fd, client_watcher* next, ev::loop_ref loop) : ev::io(loop)
		{
			std::cout<<"Client_watcher("<<main_fd<<", "<<next<<", "<<&loop<<")"<<std::endl;
			//if(!next && !loop)    throw std::runtime_error("Neither previous watcher or event_loop was passed.")
			if( -1 == main_fd )   throw std::runtime_error("Passed invalid Unix socket");
			int client_fd = accept(main_fd, NULL, NULL);
			if( -1 == client_fd ) throw std::runtime_error("Received invalid Unix socket");
			ev::io::set(client_fd, ev::READ);
			if(!next)
			{
				prev = next = this;
			}
			else // if next is valid
			{
				this->next = next;
				this->prev = next->prev;
				next->prev = this;
				prev->next = this;
			}
			start();
		}

		~client_watcher(void)
		{
			std::cout<<"~client_watcher()"<<std::endl;
			close(fd);
			stop();
			if(this != next && this != prev)
			{
				next->prev = this->prev;
				prev->next = this->next;
			}
		}

		client_watcher* prev;
		client_watcher* next;
	};
	//}}}






	const std::string socket_path;
	ev::default_loop loop;
	ev::io socket_watcher;
	//std::list<socket*> client_watchers;

	struct header_t
	{
		char doodleversion[8];
		uint16_t length;
	}  __attribute__ ((packed));

	bool read_n(int fd, char* buffer, int size, client_watcher& watcher);

	public:
		//Socket_connection(ev::loop_ref loop, const std::string& socket_path = {doodle_socket_path, sizeof(doodle_socket_path)});
		Socket_connection(const std::string& socket_path = {doodle_socket_path, sizeof(doodle_socket_path)});
		~Socket_connection(void);
		void operator()(void);
		void socket_watcher_cb(ev::io& socket_watcher, int revents);
		void client_watcher_cb(client_watcher& watcher, int revents);
};
