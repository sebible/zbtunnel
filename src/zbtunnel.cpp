#include "zbtunnel.hpp"
#include "zbconnectionmanager.hpp"

namespace zb {

	ZbTunnel::ZbTunnel(string name):name_(name)
	{
		io_service_.reset(new io_service());
	}

	ZbTunnel::ZbTunnel(string name, boost::shared_ptr<io_service>& io_service):name_(name)
	{
		this->io_service_ = io_service;
	}

	ZbTunnel::~ZbTunnel(void)
	{
		io_service_->stop();
	}

	void ZbTunnel::start_with_config(zb::config_type& config)  throw (string){
		chain_config_type conf;
		conf.push_back(config);
		start_with_config(conf);
	}
	
	void ZbTunnel::start_with_config(const ptree::ptree& config) throw (string) {
		zb::chain_config_type chain_conf;

		BOOST_FOREACH(ptree::ptree::value_type proxy, config) {
			zb::config_type conf;
			BOOST_FOREACH(ptree::ptree::value_type item, proxy.second) {
				conf[item.first] = item.second.get_value("");
			}
			chain_conf.push_back(conf);
		}

		start_with_config(chain_conf);
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

	void ZbTunnel::start_worker() {
		if (worker_ == 0 || io_service_->stopped()) {
			worker_.reset(new boost::thread(boost::bind(&ZbTunnel::worker, shared_from_this())));
		}
	}

	bool ZbTunnel::running() {
		return (worker_ && !io_service_->stopped());
	}

	void ZbTunnel::wait() {
		if (worker_.get() && !io_service_->stopped())
			worker_->join();
	}

	void ZbTunnel::stop() {
		if (manager_.get() != 0) {
			manager_->stop_all();
		}
	}

	void ZbTunnel::init() {
		if (manager_.get() == 0) {
			manager_.reset(new ZbConnectionManager());
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

	///////////////////////////
	ZbSocketTunnel::ZbSocketTunnel(string name):ZbTunnel(name), old_local_port_(0), local_port_(0) {
	}

	ZbSocketTunnel::ZbSocketTunnel(string name, shared_ptr<io_service>& io_service):ZbTunnel(name, io_service), old_local_port_(0), local_port_(0) {
	}

	ZbSocketTunnel::~ZbSocketTunnel() {
	}

	void ZbSocketTunnel::start_with_config(zb::chain_config_type& config)  throw (string){
		config_ = config;
		if (config.size() == 0)
			throw string("There has to be at least 1 config");

		config_type& conf0 = config[0];
		old_local_port_ = local_port_;
		old_local_address_ = local_address_;
		local_port_ = CONFIG_GET_INT(conf0, "local_port", 8080); 
		local_address_ = CONFIG_GET(conf0, "local_address", "0.0.0.0"); 

		start();
	}

	void ZbSocketTunnel::start() {
		init();
		start_accept();

		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbSocketTunnel", name_ + ": Starting on " + local_address_ + ":" + boost::lexical_cast<string>(local_port_));
	}

	void ZbSocketTunnel::stop() {
		if (acceptor_.get() != 0) {
			gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbSocketTunnel", name_ + ": Acceptor stopped");
			acceptor_->close();
			acceptor_.reset();
		}

		ZbTunnel::stop();
	}

	void ZbSocketTunnel::init() {
		ZbTunnel::init();

		if (acceptor_.get() != 0 && (old_local_port_ != local_port_ || old_local_address_.compare(local_address_) != 0)) {
			acceptor_->close();
		}

		if (acceptor_.get() == 0 || !acceptor_->is_open()) {
			try {
				boost::asio::ip::address addr = boost::asio::ip::address::from_string(local_address_);				
				acceptor_.reset(new tcp::acceptor(*io_service_, tcp::endpoint(addr, local_port_)));
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbSocketTunnel", name_ + ": Acceptor reseted");
			} catch (std::exception& e) {
				gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbSocketTunnel", name_ + ": Unable to bind to local address");
				throw e;
			}
		}
	}

	void ZbSocketTunnel::start_accept()
	{
		socket_ptr socket(new tcp::socket(*io_service_));
		ZbSocketTransport::pointer tp(new ZbSocketTransport(socket, io_service_));
		
		acceptor_->async_accept(*socket,
			bind(&ZbSocketTunnel::handle_accept, boost::static_pointer_cast<ZbSocketTunnel>(shared_from_this()), tp,
				boost::asio::placeholders::error));

		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbSocketTunnel", name_ + ": Accepting...");
	}

	void ZbSocketTunnel::handle_accept(ZbSocketTransport::pointer& in,
		const error_code& error)
	{
		if (!error)
		{
			gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_DEBUG, "ZbSocketTunnel", name_ + ": Accpeted a new connection ...");
			assert(manager_.get() != 0);

			ZbConnection::pointer conn = manager_->get_or_create_conn(io_service_, shared_from_this());
			conn->start(in);
			start_accept();
		} else {
			gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbSocketTunnel", name_ + ": Stop accepting:" + error.message());
		}
	}

	///////////////////////////////////////
	ZbIoTunnel::ZbIoTunnel(string name):ZbTunnel(name) {
	}

	ZbIoTunnel::ZbIoTunnel(string name, shared_ptr<io_service>& io_service):ZbTunnel(name, io_service) {
	}

	void ZbIoTunnel::start_with_config(zb::chain_config_type& config)  throw (string){
		config_ = config;
		if (config.size() == 0)
			throw string("There has to be at least 1 config");

		start();
	}

	void ZbIoTunnel::start() {
		init();

		assert(manager_.get() != 0);
		ZbConnection::pointer conn = manager_->get_or_create_conn(io_service_, shared_from_this());
		ZbStreamTransport::pointer tp(new ZbStreamTransport(io_service_));
		conn->start(tp);

		gconf.log(gconf_type::DEBUG_TUNNEL, gconf_type::LOG_WARN, "ZbIoTunnel", name_ + ": Starting on stdin");
	}

	void ZbIoTunnel::stop() {
		ZbTunnel::stop();
	}

	void ZbIoTunnel::init() {
		ZbTunnel::init();
	}

}