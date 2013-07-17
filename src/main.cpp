#include "zbtunnel.hpp"
#include <signal.h>

#include <iostream>

namespace zb {

	string banner() {
		return string(DISPLAY_NAME " " VERSION 
	#ifdef WITH_OPENSSL
		" openssl" 
	#endif
	#ifndef WIN32
	#ifndef DISABLE_EPOLL
		" epoll"
	#endif
	#endif
		"\n");
	}

	string usage(string exe) {
		return string("Usage:\n\t") + exe + " [-] <config_filename>\n";
	}

	void quit(int code) {
	#ifdef DEBUG
		std::cerr << "Press any key to continue...\n";
		getchar();
	#endif
		exit(code);
	}

	class ZbTunnelMain {
	protected:
		typedef std::map<string, ZbTunnel::pointer> tunnel_map;
		boost::thread_group tg_;
		tunnel_map tunnels_;
		io_service service_;
		boost::asio::signal_set signals_;
		std::ostream *out_, *err_;

	public:
		ZbTunnelMain():signals_(service_) {}

		int run(int argc, char **argv) {
			out_ = &std::cout;
			err_ = &std::cerr;
			string filename;

			if (argc < 2) {
				*err_ << banner() << usage(argv[0]);
				quit(1);
			} else if (string(argv[1]).compare("-") == 0) {
				// Prepare stdin
				std::ios_base::sync_with_stdio(false);
				std::cin.tie((std::ostream*)0);
				std::cerr.tie((std::ostream*)0);

				out_ = err_;
				gconf.out(out_);
				if (argc < 3) {
					*err_ << banner() << usage(argv[0]);
					quit(1);
				}
				filename = argv[2];
			} else {
				filename = argv[1];
			}

			signals_.add(SIGINT);
			signals_.add(SIGTERM);
#ifdef SIGBREAK
			signals_.add(SIGBREAK);
#endif
#ifdef SIGQUIT
			signals_.add(SIGQUIT);
#endif
			signals_.async_wait(boost::bind(&ZbTunnelMain::stop, this));
			
			try {
				*out_ << banner();
				*out_ << "Loading conf: " << filename << "\n";

				ptree::ptree pt, empty;
				ptree::json_parser::read_json(filename, pt);
				
				BOOST_FOREACH(ptree::ptree::value_type node, pt) {

					if (node.first.compare("global") == 0) {
						const ptree::ptree& global = node.second;
						gconf.log_filter(global.get("log_filter", gconf.log_filter()));
						gconf.log_level((zb::ZbConfig::log_level_type)global.get("log_level", (int)gconf.log_level()));
						gconf.allow_reuse(global.get<bool>("log_level", gconf.allow_reuse()));
						gconf.preconnect(global.get("preconnect", (int)gconf.preconnect()));
					} else if (node.first.compare("-") == 0) {
						if (tunnels_.size() > 0) 
							throw "The io tunnel should be the only tunnel in the config";
				
						boost::shared_ptr<zb::ZbTunnel> t(new zb::ZbIoTunnel(node.first));
						tunnels_[node.first] = t;
						t->start_with_config(node.second);
						boost::thread* tr = new boost::thread(t->get_worker_func());
						tg_.add_thread(tr);
						break;

					} else {
						boost::shared_ptr<zb::ZbTunnel> t(new zb::ZbSocketTunnel(node.first));
						tunnels_[node.first] = t;
						t->start_with_config(node.second);
						boost::thread* tr = new boost::thread(t->get_worker_func());
						tg_.add_thread(tr);
					}
				}
		
				if (tunnels_.size() == 0) {
					*err_ << "No tunnel definition is found. Abort.\n";
					quit(2);
				}

				boost::thread(boost::bind(&ZbTunnelMain::wait_for_threads, this));
				service_.run();
				*out_ << "ZbTunnel finished.\n";

			} catch (ptree::ptree_error& e) {
				*err_ << e.what() << "\n";
				quit(2);
			} catch (std::exception& e) {
				*err_ << e.what() << "\n";
		#ifdef DEBUG
				throw;
		#endif
				quit(2);
			} catch (string& e) {
				*err_ << e << "\n";
				quit(2);
			}

			return 0;
		}

		void wait_for_threads() {
			tg_.join_all();
			raise(SIGTERM);
		}

		void stop() {
			BOOST_FOREACH(tunnel_map::value_type& node, tunnels_) {
				assert(node.second.get() != 0);
				node.second->stop();
			}

			tg_.join_all();
			*out_ << "ZbTunnel finished.\n";
			gconf.flush();
			out_->flush();
			err_->flush();
			quit(0);
		}
	};
}


int main(int argc, char **argv) {
	return zb::ZbTunnelMain().run(argc, argv);
}
