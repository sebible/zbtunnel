#include "zbtunnel.hpp"

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


namespace ptree = boost::property_tree;
using std::cout;

int main(int argc, char **argv) {
	cout << "ZbTunnel " << VERSION << "\n";

	if (argc < 2) {
		cout << "Usage:\n\tzbtunnel config_filename\n";
		exit(0);
	}

	try {
		cout << "Loading conf: " << argv[1] << "\n";
		ptree::ptree pt, empty;
		ptree::json_parser::read_json(argv[1], pt);
		
		boost::thread_group tg;
		std::set<zb::ZbTunnel::pointer> tunnels;

		BOOST_FOREACH(ptree::ptree::value_type node, pt) {

			if (node.first.compare("global") == 0) {
				const ptree::ptree& global = node.second;
				gconf.log_filter(global.get("log_filter", gconf.log_filter()));
				gconf.log_level((zb::ZbConfig::log_level_type)global.get("log_level", (int)gconf.log_level()));
				gconf.allow_reuse(global.get<bool>("log_level", gconf.allow_reuse()));
			} else {
				zb::chain_config_type chain_conf;
				const ptree::ptree& chain = node.second;

				BOOST_FOREACH(ptree::ptree::value_type proxy, chain) {
					zb::config_type conf;
					BOOST_FOREACH(ptree::ptree::value_type item, proxy.second) {
						conf[item.first] = item.second.get_value("");
					}
					chain_conf.push_back(conf);
				}

				boost::shared_ptr<zb::ZbTunnel> t(new zb::ZbTunnel(node.first));
				tunnels.insert(t);
				t->start_with_config(chain_conf, true);
				tg.add_thread(t->get_worker().get());
			}
		}

		if (tunnels.size() == 0) {
			cout << "No tunnel definition is found. Abort.\n";
			exit(2);
		}

		tg.join_all();

	} catch (ptree::ptree_error& e) {
		cout << e.what();
		exit(1);
	} catch (std::exception& e) {
		cout << e.what();
		exit(1);
	}
	
	return 0;
}