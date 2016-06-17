// compile with g++ timer.cpp -lev
#pragma once
#include <ev++.h>




class Socket_connection
{
	ev::default_loop loop;
	ev::io socket_watcher;

	public:
		Socket_connection(const std::string& socket_path = "./socket");
		void operator()(void);

		void socket_watcher_cb(ev::io& socket_watcher, int revents);
		void client_watcher_cb(ev::io& socket_watcher, int revents);

};
