#pragma once

#include "zbtunnel/zbconfig_inc.hpp"

#define UA_STRING DISPLAY_NAME "/" VERSION

#ifdef DISABLE_EPOLL
#define BOOST_ASIO_DISABLE_EPOLL
#endif

#if defined(__CYGWIN__) || defined(WIN32)
#  define _WIN32_WINNT 0x0501 
#endif 

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifdef WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#endif

#include <string>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <set>
#include <map>
#include <vector>

using boost::asio::ip::tcp;
using boost::asio::io_service;
using boost::system::error_code;
using boost::system::make_error_code;
namespace errc = boost::system::errc;
namespace ptree = boost::property_tree;
namespace chrono = boost::chrono;
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::weak_ptr;
using std::vector;
using std::map;
using std::string;

#define ZB_GETTER_SETTER(name, type) void name(type v) {name##_ = v;}; type name() {return name##_;}

namespace zb {
	namespace tunnel {
		typedef std::map<std::string,std::string> config_type;
		typedef std::vector<config_type> chain_config_type;
	}
}