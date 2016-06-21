// compile with g++ timer.cpp -lev
#pragma once
#include <ev++.h>
#include <iostream>



//{{{

namespace ev
{
	struct socket : ev::io
	{
		using ev::io::io;
		socket(int fd, int events, ev::loop_ref loop) : ev::io(loop)
		{
			if( -1 == fd ) throw std::runtime_error("Received invalid Unix socket");
			ev::io::set(fd, events);
		}

		~socket(void) { std::cout<<"~ev::socket()"<<std::endl; }

		int some_data;		// A data member for demonstration purposes
		// Put any additional data here



		//{{{ Template Magic to allow using a callback with signature "void socket_watcher_cb(ev::socket& socket_watcher, int revents);"

		template < class K, void (K::* method)(ev::socket& w, int) >
		static void method_thunk(struct ev_loop* loop, ev_watcher* w, int revents)
		{
			(void) loop;
			(static_cast < K*> (w->data)->*method)(*reinterpret_cast<ev::socket*>(w), revents);
		}

		template<class K, void (K::* method)(ev::socket& w, int)>
		void set(K* object) throw()
		{
			this->data = (void*) object;
			this->cb = reinterpret_cast<void (*)(struct ev_loop*, struct ev_io*, int)>(&method_thunk < K, method >);
		}
		//}}}
	};
}
//}}}

class Socket_connection
{
	const std::string& socket_path;
	ev::default_loop loop;
	ev::socket socket_watcher;

	struct header_t
	{
		char doodleversion[8];
		uint16_t length;
	}  __attribute__ ((packed));

	bool read_n(int fd, char* buffer, int size, ev::socket& client_watcher);

	public:
		//Socket_connection(const std::string& socket_path = "./socket");
		Socket_connection(const std::string& socket_path = "\0hidden");
		void operator()(void);
		void socket_watcher_cb(ev::socket& socket_watcher, int revents);
		void client_watcher_cb(ev::socket& socket_watcher, int revents);
};
