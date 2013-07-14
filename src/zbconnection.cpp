#include "zbconnection.hpp"
#include "zbtunnel.hpp"
#include "zbconnectionmanager.hpp"

namespace zb {
	ZbConnection::pointer ZbConnection::create(shared_ptr<io_service>& service, client_ptr client)
	{
		pointer p(new ZbConnection());
	
		p->client_ = client;
		p->out_.reset(new ZbSocketTransport(socket_ptr(new tcp::socket(*service)), service));

		return p->shared_from_this();
	}

	ZbConnection::ZbConnection():state_(INIT),current_(0)
	{
	}

	string ZbConnection::to_string() {
		std::stringstream s;
		s << "#" << boost::lexical_cast<string>(out_.get()) << "." << current_;
		return s.str();
	}

	string ZbConnection::_state_throw(string msg) {
		state_ = BAD;
		stop(false);
		throw to_string() + " error: " + msg;
		return "";
	}

	void ZbConnection::start(const ZbTransport::pointer& in)
	{
		assert(in_.get() == 0);
		in_ = in;

		if (state_ == CONNECTED) {
			// Start transfer right away;
			handle_init(error_code());
			return;
		}

		shared_ptr<ZbTunnel> c = client_.lock();
		assert(c.get() != 0);

		gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", string("Starting ") + to_string());
		try {
			// Create connection to server
			if (c->endpoint_cache_.get() != 0) {
				gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" is making first connection using cache"));
				out_->async_connect(*(c->endpoint_cache_), bind(&ZbConnection::handle_connect, shared_from_this(), _1));
			} else {
				string host = CONFIG_GET(c->config_[0], "host", STATETHROW("host missing in conf0"));
				string port = CONFIG_GET(c->config_[0], "port", STATETHROW("port missing in conf0"));
				gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" is making first connection to ") + host + ":" + port);
				out_->async_connect(host, port, bind(&ZbConnection::handle_connect, shared_from_this(), _1));
			}
		} catch (std::exception &e) {
			// Error connecting to remote
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", string("Start failed.") + e.what());
			c->last_error_ = e.what();
			stop(false);
			return;
		}
	}

	void ZbConnection::stop(bool reusable)
	{
		string err1 = in_.get() ? in_->last_error() : "";
		string err2 = out_->last_error();

		reusable = reusable && gconf.allow_reuse();

		if (in_.get() == 0) {
			reusable = false;
		} else {
			in_->close();
			in_.reset();
		}

		ZbTunnel::pointer c = client_.lock();
		ZbConnectionManager::pointer m;
		if (c.get() != 0) m = c->manager_;

		if (reusable && state_ == CONNECTED && out_->is_open() && out_->last_error().empty() && m.get() != 0) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" recycled") + (err1.empty() ? "" : " with error:"));
			m->recycle(shared_from_this());
		}
		else {
			if (m.get() != 0) m->remove(shared_from_this());
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" stopped") + ((err1.empty() && err2.empty()) ? "" : " with error:"));
			if (!err1.empty()) gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", string("in: ") + err1);
			if (!err2.empty()) gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_WARN, "ZbConnection", string("out: ") + err2);
			out_->close();
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

		if (c->endpoint_cache_.get() == 0) {
			ZbSocketTransport* tp = dynamic_cast<ZbSocketTransport*>(out_.get());
			if (tp) c->endpoint_cache_.reset(new tcp::endpoint(tp->get_endpoint()));
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
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", "Connected, start to transfer");
			in_->async_receive(buf_[0][0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, 0));		
			out_->async_receive(buf_[1][0], BUFSIZE, bind(&ZbConnection::handle_transfer, shared_from_this(), _1, _2, 1));		
		}
	}

	void ZbConnection::handle_transfer(const error_code& error, size_t size, int direction) {
		if (error) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", error.message() + boost::lexical_cast<string>(direction));
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