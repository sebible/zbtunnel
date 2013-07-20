#include "zbconnection.hpp"
#include "zbtunnel.hpp"
#include "zbconnectionmanager.hpp"
#include "zbtransport.hpp"

namespace zb {
	ZbConnection::pointer ZbConnection::create(shared_ptr<io_service>& service, client_ptr client)
	{
		pointer p(new ZbConnection());
	
		p->client_ = client;
		socket_ptr socket(new tcp::socket(*service));
		p->out_.reset(new ZbSocketTransport(socket, service));

		return p->shared_from_this();
	}

	ZbConnection::ZbConnection():state_(INIT),current_(0)
	{
	}

	string ZbConnection::to_string() {
		std::stringstream s;
		s << owner_ << "#" << boost::lexical_cast<string>(id_) << "." << current_;
		return s.str();
	}

	string ZbConnection::_state_throw(string msg) {
		state_ = BAD;
		stop(false);
		throw to_string() + " error: " + msg;
		return "";
	}

	void ZbConnection::start() {
		start(ZbTransport::pointer());
	}

	template <typename TransportPointer>
	void ZbConnection::start(TransportPointer in)
	{
		in_ = in;

		if (state_ == CONNECTED) {
			// Start transfer right away;
			assert(in_.get() != 0);
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_INFO, "ZbConnection", to_string() + string(" reused, starting to transfer"));
			if (in_.get()) in_->async_receive(buf_[0][0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, 0));	
			return;
		}

		if (state_ == CONNECTING) {
			assert(in_.get() != 0);
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_INFO, "ZbConnection", to_string() + string(" reused, connecting in progress"));
			// Connecting in progress. do nothing. 
			return;
		}
		
		gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" starting") + (in_.get() == 0 ? " for preconnecting" : ""));

		shared_ptr<ZbTunnel> c = client_.lock();
		assert(c.get() != 0);

		try {
			state_ = CONNECTING;
			// Create connection to server
			if (c->endpoint_cache_.get() != 0) {
				gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" is making first connection to cached endpoint"));
				out_->async_connect(*(c->endpoint_cache_), bind(&ZbConnection::handle_connect, shared_from_this(), _1));
			} else {
				string host = CONFIG_GET(c->config_[0], "host", STATETHROW("host missing in conf0"));
				string port = CONFIG_GET(c->config_[0], "port", STATETHROW("port missing in conf0"));
				gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" is making first connection to ") + host + ":" + port);
				out_->async_connect(host, port, bind(&ZbConnection::handle_connect, shared_from_this(), _1));
			}
		} catch (std::exception &e) {
			// Error connecting to remote
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", string("Start failed. ") + e.what());
			c->last_error_ = e.what();
			stop(false);
			return;
		}
	}

	void ZbConnection::stop(bool recycle, bool remove)
	{
		if (in_.get() == 0 && (out_.get() == 0 || !out_->is_open())) return;

		string err1 = in_.get() ? in_->last_error() : "";
		string err2 = out_->last_error();

		if (in_.get() == 0) {
			recycle = false;
		} else {
			in_->close();
			in_.reset();
		}

		ZbTunnel::pointer c = client_.lock();
		ZbConnectionManager::pointer m;
		if (c.get() != 0) m = c->manager_;
		assert(m.get() != 0);

		bool reused = false;
		if (recycle && m->recycle() && state_ == CONNECTED && out_->is_open() && out_->last_error().empty()) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_INFO, "ZbConnection", to_string() + string(" to be recycled"));
			reused = m->recycle(shared_from_this());
		}
		
		if (!reused) {
			if (remove) m->remove(shared_from_this());
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_INFO, "ZbConnection", to_string() + string(" stopped") + ((err1.empty() && err2.empty()) ? "" : " with error"));
			if (!err1.empty()) gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", string("in: ") + err1);
			if (!err2.empty()) gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", string("out: ") + err2);
			out_->close();
			// Hold out_ in case there are some async ops to be finished
		}
	}

	void ZbConnection::handle_connect(const error_code& error) {
		if (error) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", to_string() + string(" connect error: ") + error.message());
			stop(false);
			return;
		} else {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" connected."));
		}

		config_type conf;
		ZbTunnel::pointer c = client_.lock();
		assert(c.get() != 0);

		if (current_ == 0) {
			if (c->endpoint_cache_.get() == 0) {
				ZbSocketTransport* tp = dynamic_cast<ZbSocketTransport*>(out_.get());
				if (tp) c->endpoint_cache_.reset(new tcp::endpoint(tp->get_endpoint()));
			}
		}
		
		if ((int)c->config_.size() > current_) {
			conf = c->config_[current_];
			string ttype = CONFIG_GET(conf, "transport", STATETHROW("transport missing in conf"));
		
			try {
				if (ttype.compare("shadow") == 0)
					out_.reset(new ZbShadowTransport(out_, conf));
				else if (ttype.compare("http") == 0)
					out_.reset(new ZbHttpTransport(out_, conf));
#ifdef WITH_OPENSSL
				else if (ttype.compare("https") == 0)
					out_.reset(new ZbHttpsTransport(out_, conf));
#else
				else if (ttype.compare("https") == 0)
					THROW(string("https is only available when compiled with openssl"));
#endif
				else if (ttype.compare("socks5") == 0)
					out_.reset(new ZbSocks5Transport(out_, conf));
				else if (ttype.compare("raw") == 0) {
					// do nothing
					handle_init(error_code());
					return;
				}
				else
					THROW(string("unsupported transport type: ") + ttype);
			} 
			catch (string& e) {
				// This will properly end the connection
				STATETHROW(e);
			}

			out_->init(boost::bind(&ZbConnection::handle_init, shared_from_this(), _1));
		} else {
			// Start transfer
			handle_init(error_code());
		}
	}

	/// Init a new transport
	void ZbConnection::handle_init(const error_code& error) {
		if (error) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", to_string() + string(" init error: ") + error.message());
			stop(false);
			return;
		} else {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" init succeeded."));
		}

		config_type conf;
		ZbTunnel::pointer c = client_.lock();
		assert(c.get() != 0);

		if ((int)c->config_.size() > current_ + 1) {
			conf = c->config_[current_ + 1];
			string host = CONFIG_GET(conf, "host", STATETHROW("host missing in conf"));
			string port = CONFIG_GET(conf, "port", STATETHROW("port missing in conf"));
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" is connecting to ") + host + ":" + port);
			out_->async_connect(host, port, bind(&ZbConnection::handle_connect, shared_from_this(), _1));
			current_++;
		} else {
			// Start transfer
			state_ = CONNECTED;
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_INFO, "ZbConnection", to_string() + " connected, " + (in_.get() == 0 ? "waiting for incoming connection" : "starting to transfer"));
			if (in_.get()) in_->async_receive(buf_[0][0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, 0));		
			out_->async_receive(buf_[1][0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, 1));		
		}
	}

	void ZbConnection::handle_transfer(const error_code& error, size_t size, int direction) {
		if (error) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + " dir:" + boost::lexical_cast<string>(direction) + " transfer interrupted:" + error.message());
			stop(direction == 0); 
			return;
		}

		shared_ptr<ZbTunnel> c = client_.lock();
		if (c.get() == 0) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", "Unable to get client.");
			stop(false);
			return;
		}

		buf_type& buf = buf_[direction];
		memcpy(buf[1], buf[0], size);
		if (direction == 0) {
			out_->async_send(buf[1], size, bind(&ZbConnection::handle_write, shared_from_this(), _1, _2, direction));
			if (in_.get() != 0) in_->async_receive(buf[0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, direction));		
		} else {
			if (in_.get() != 0) in_->async_send(buf[1], size, bind(&ZbConnection::handle_write,	shared_from_this(), _1, _2, direction));
			out_->async_receive(buf[0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, direction));		
		}
	}

	void ZbConnection::handle_write(const boost::system::error_code& error, size_t bytes_transferred, int direction)
	{
		if (error) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", string("Error writing:") + error.message());
			stop(direction == 0);
			return;
		}
	}

}