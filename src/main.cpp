#include "zbtunnel.hpp"

#include <iostream>

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

int main(int argc, char **argv) {
	std::ostream* out = &std::cout;
	std::ostream* err = &std::cerr;
	string filename;

	if (argc < 2) {
		*err << banner() << usage(argv[0]);
		quit(1);
	} else if (string(argv[1]).compare("-") == 0) {
		// Prepare stdin
		std::ios_base::sync_with_stdio(false);
		std::cin.tie((std::ostream*)0);
		std::cerr.tie((std::ostream*)0);

		out = err;
		gconf.output(out);
		if (argc < 3) {
			*err << banner() << usage(argv[0]);
			quit(1);
		}
		filename = argv[2];
	} else {
		filename = argv[1];
	}

	try {
		*out << banner();
		*out << "Loading conf: " << filename << "\n";

		ptree::ptree pt, empty;
		ptree::json_parser::read_json(filename, pt);
	
		boost::thread_group tg;
		std::set<zb::ZbTunnel::pointer> tunnels;

		BOOST_FOREACH(ptree::ptree::value_type node, pt) {

			if (node.first.compare("global") == 0) {
				const ptree::ptree& global = node.second;
				gconf.log_filter(global.get("log_filter", gconf.log_filter()));
				gconf.log_level((zb::ZbConfig::log_level_type)global.get("log_level", (int)gconf.log_level()));
				gconf.allow_reuse(global.get<bool>("log_level", gconf.allow_reuse()));
			} else if (node.first.compare("-") == 0) {
				if (tunnels.size() > 0) 
					throw "The io tunnel should be the only tunnel in the config";
				
				boost::shared_ptr<zb::ZbTunnel> t(new zb::ZbIoTunnel(node.first));
				tunnels.insert(t);
				t->start_with_config(node.second);
				boost::thread* tr = new boost::thread(t->get_worker_func());
				tg.add_thread(tr);
				break;

			} else {
				boost::shared_ptr<zb::ZbTunnel> t(new zb::ZbSocketTunnel(node.first));
				tunnels.insert(t);
				t->start_with_config(node.second);
				boost::thread* tr = new boost::thread(t->get_worker_func());
				tg.add_thread(tr);
			}
		}
		
		if (tunnels.size() == 0) {
			*err << "No tunnel definition is found. Abort.\n";
			exit(2);
		}

		tg.join_all();

	} catch (ptree::ptree_error& e) {
		*err << e.what() << "\n";
		quit(2);
	} catch (std::exception& e) {
		*err << e.what() << "\n";
		quit(2);
	} catch (string& e) {
		*err << e << "\n";
		quit(2);
        }

	return 0;
}
