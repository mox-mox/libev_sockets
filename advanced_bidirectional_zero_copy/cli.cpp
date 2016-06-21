// compile with g++ timer.cpp -lev
#include <ev++.h>
#include <iostream>
#include <queue>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev++.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <bitset>

const std::string socket_path = "./socket";




//{{{ Extend an IO watcher with a wrtie-queue
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
		socket(int fd, int events)
		{
			if( -1 == fd ) throw std::runtime_error("Received invalid Unix socket");
			ev::io::set(fd, events);
		}

		std::deque<std::string> write_data;

		//{{{ Template Magic to allow using callbacks with "void socket_watcher_cb(ev::socket& socket_watcher, int revents);"

		//{{{
		template < class K, void (K::* method)(ev::socket& w, int) >
		static void method_thunk(struct ev_loop* loop, ev_watcher* w, int revents)
		{
			(void) loop;
			(static_cast < K*> (w->data)->*method)(*reinterpret_cast<ev::socket*>(w), revents);
		}

		template < class K, void (K::* method)(ev::socket& w, int) >
		void set(K* object) throw()
		{
			this->data = (void*) object;
			this->cb = reinterpret_cast<void (*)(struct ev_loop*, struct ev_io*, int)>(&method_thunk < K, method >);
		}
		//}}}

		//{{{
		template < void (* function)(ev::socket& w, int) >
		static void function_thunk(struct ev_loop* loop, ev_watcher* w, int revents)
		{
			(void) loop;
			function(*reinterpret_cast<ev::socket*>(w), revents);
		}

		template < void (* function)(ev::socket& w, int) >
		void set(void* data = 0) throw()
		{
			this->data = data;
			this->cb = reinterpret_cast<void (*)(struct ev_loop*, struct ev_io*, int)>(&function_thunk < function >);
		}
		//}}}

		//}}}
	};

	socket& operator<<(socket& lhs, const std::string& data)
	{
		lhs.write_data.push_back(data);
		if(!lhs.is_active()) lhs.start();

		return lhs;
	}
}
//}}}




//{{{
void stdin_cb(ev::io& w, int revent)
{
	(void) revent;

	std::string buf;
	std::cin>>buf;
	std::cout<<"Writing \""<<buf<<"\"."<<std::endl;
	static constexpr auto magic("Doodle01");
	uint16_t length = buf.length();
	std::string credential(magic, 0, sizeof(magic+2));
	credential.append(static_cast<char*>(static_cast<void*>(&length)), 2);

	(*reinterpret_cast<ev::socket*>(w.data))<<credential<<buf;
}
//}}}




void write_n(int fd, char buffer[], int size)	// Write exactly size bytes
{
	int write_count = 0;
	while(write_count < size)
	{
		int n;
		switch((n=write(fd, &buffer[write_count], size-write_count)))
		{
			case -1:
				throw std::runtime_error("Write error on the connection using fd." + std::to_string(fd) + ".");
			case  0:
				std::cout<<"Received EOF (Client has closed the connection)."<<std::endl;
				throw std::runtime_error("Write error on the connection using fd." + std::to_string(fd) + ".");
				return;
			default:
				write_count+=n;
		}
	}
}




//{{{
void socket_cb(ev::socket& w, int revent)
{
	(void) revent;
	if(w.write_data.empty())
	{
		w.stop();
	}
	else
	{
		std::string buf = w.write_data.front();
		w.write_data.pop_front();
		write_n(w.fd, &buf[0], buf.length());
	}
}
//}}}

//{{{
void socket_stat_cb(ev::stat& w, int revent)
{
	(void) revent;
	std::cout<<"Connection was closed. Stoping program..."<<std::endl;
	w.loop.break_loop(ev::ALL);
}
//}}}







int main(void)
{
	ev::default_loop loop;

	//{{{ Standard Unix socket creation

	ev::socket socket_watcher(socket(AF_UNIX, SOCK_STREAM, 0), ev::WRITE, loop);
	socket_watcher.set<socket_cb>(nullptr);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	if(socket_path.length() >= sizeof(addr.sun_path)-1)
	{
		throw std::runtime_error("Unix socket path \"" + socket_path + "\" is too long. "
		                         "Maximum allowed size is " + std::to_string(sizeof(addr.sun_path)) + "." );
	}
	std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

	if( connect(socket_watcher.fd, (struct sockaddr*) &addr, sizeof(addr)) == -1 )
	{
		throw std::runtime_error("Could not connect to socket "+socket_path+".");
	}

	socket_watcher.start();


	ev::stat socket_stat_watcher(loop);
	socket_stat_watcher.set<socket_stat_cb>(static_cast<void*>(&socket_watcher));
	socket_stat_watcher.start(socket_path.c_str(), 0);


	//}}}

	//////{{{ Create a libev io watcher to respond to terminal input

	ev::io stdin_watcher(loop);
	stdin_watcher.set<stdin_cb>(static_cast<void*>(&socket_watcher));
	stdin_watcher.set(STDIN_FILENO, ev::READ);
	stdin_watcher.start();
	//////}}}

	loop.run();

	return 0;
}
