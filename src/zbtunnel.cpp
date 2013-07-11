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

	socket_ptr ZbConnection::socket()
	{
		if (in_.get() == 0) 
			return socket_ptr();

		return in_->socket();
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

	void ZbConnection::start(ZbSocketTransport::pointer& in)
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
		ZbConnectionManager *m = ZbConnectionManager::get_instance();
		assert(m != 0);

		string err1 = in_.get() ? in_->last_error() : "";
		string err2 = out_->last_error();

		reusable = reusable && gconf.allow_reuse();

		if (in_.get() == 0) {
			reusable = false;
		} else {
			in_->close();
			in_.reset();
		}

		if (reusable && state_ == CONNECTED && out_->is_open() && out_->last_error().empty()) {
			gconf.log(gconf_type::DEBUG_CONNECTION, gconf_type::LOG_DEBUG, "ZbConnection", to_string() + string(" recycled") + (err1.empty() ? "" : " with error:"));
			m->recycle(shared_from_this());
		}
		else {
			m->remove(shared_from_this());
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
	#ifdef WITH_HTTPS
				else if (ttype.compare("https") == 0)
					out_.reset(new ZbHttpsTransport(out_, conf));
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

	/////////////////////////////
	ZbTunnel::ZbTunnel(string name):name_(name), old_local_port_(0), local_port_(0)
	{
		io_service_.reset(new io_service());
	}

	ZbTunnel::ZbTunnel(string name, boost::shared_ptr<io_service>& io_service):name_(name), old_local_port_(0), local_port_(0)
	{
		this->io_service_ = io_service;
	}

	ZbTunnel::~ZbTunnel(void)
	{
		io_service_->stop();
	}

	void ZbTunnel::start_with_config(zb::config_type& config, bool threaded)  throw (string){
		chain_config_type conf;
		conf.push_back(config);
		start_with_config(conf, threaded);
	}

	void ZbTunnel::start_with_config(zb::chain_config_type& config, bool threaded)  throw (string){
		config_ = config;
		if (config.size() == 0)
			throw string("There has to be at least 1 config");

		config_type& conf0 = config[0];
		old_local_port_ = local_port_;
		old_local_address_ = local_address_;
		local_port_ = CONFIG_GET_INT(conf0, "local_port", 8080); 
		local_address_ = CONFIG_GET(conf0, "local_address", "0.0.0.0"); 

		init();
		start_accept();

		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": Starting on " + local_address_ + ":" + boost::lexical_cast<string>(local_port_));

		if (!threaded) return;

		if (worker_ == 0 || io_service_->stopped()) {
			worker_.reset(new boost::thread(boost::bind(&ZbTunnel::worker, shared_from_this())));
		}
	}

	void ZbTunnel::worker() {
		assert(io_service_.get() != 0);
		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbTunnel", name_ + ": Worker started");
		while(!io_service_->stopped()) {
			try {
				io_service_->run();
			}
			catch (const string& e) {
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": " + e);
				last_error_ = e;
			}
			catch (const std::exception& e) {
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": " + e.what());
				last_error_ = e.what();
			}
			catch (...) {
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": worker crashed");
			}
		}
		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": service exited");
	}

	void ZbTunnel::stop() {
		if (acceptor_) {
			gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbTunnel", name_ + ": Acceptor stopped");
			acceptor_->close();
			acceptor_.reset();
		}
	}

	bool ZbTunnel::running() {
		return (worker_ && !io_service_->stopped());
	}

	void ZbTunnel::wait() {
		if (worker_.get() && !io_service_->stopped())
			worker_->join();
	}

	void ZbTunnel::init() {
		if (acceptor_.get() != 0 && (old_local_port_ != local_port_ || old_local_address_.compare(local_address_) != 0)) {
			acceptor_->close();
		}

		if (acceptor_.get() == 0 || !acceptor_->is_open()) {
			try {
				boost::asio::ip::address addr = boost::asio::ip::address::from_string(local_address_);				
				acceptor_.reset(new tcp::acceptor(*io_service_, tcp::endpoint(addr, local_port_)));
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbTunnel", name_ + ": Acceptor reseted");
			} catch (std::exception& e) {
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": Unable to bind to local address");
				throw e;
			}
		}

		init_coders();
	}

	void ZbTunnel::init_coders() {
		ZbCoderPool* cp = ZbCoderPool::get_instance();
		assert(cp != 0);

		BOOST_FOREACH(chain_config_type::value_type& conf, config_) {
			string transport = CONFIG_GET(conf, "transport", "");
			if (transport.compare("shadow") == 0) {
				string method = CONFIG_GET(conf, "method", "");
				string key = CONFIG_GET(conf, "key", "");
				if (key.empty()) continue;
				cp->get_coder(method, key);
			}
		}
	}

	void ZbTunnel::start_accept()
	{
		ZbSocketTransport::pointer tp(new ZbSocketTransport(socket_ptr(new tcp::socket(*io_service_)), io_service_));

		socket_ptr s = tp->socket();
		assert(s.get() != 0);

		acceptor_->async_accept(*s,
			bind(&ZbTunnel::handle_accept, shared_from_this(), tp,
				boost::asio::placeholders::error));

		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbTunnel", name_ + ": Accepting...");
	}

	void ZbTunnel::handle_accept(ZbSocketTransport::pointer& in,
		const error_code& error)
	{
		if (!error)
		{
			gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbTunnel", name_ + ": Accpeted a new connection ...");
			ZbConnectionManager *m = ZbConnectionManager::get_instance();
			assert(m != 0);

			ZbConnection::pointer conn = m->get_or_create_conn(name_, io_service_, shared_from_this());
			conn->start(in);
			start_accept();
		} else {
			gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbTunnel", name_ + ": Stop accepting:" + error.message());
		}
	}

}