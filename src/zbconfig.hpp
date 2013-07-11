#pragma once

#define WITH_HTTPS

//#define DEBUG_SOCKET_TRANSPORT
//#define DEBUG_SHADOW_TRANSPORT
//#define DEBUG_HTTP_TRANSPORT
//#define DEBUG_SOCKS_TRANSPORT
#define UA_STRING "ZbTunnel/1.0"
/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2013 yufeiwu@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/

#include <boost/asio.hpp>
#ifdef WITH_HTTPS
#include <boost/asio/ssl.hpp>
#endif
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
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
#define ERRC boost::system::errc
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::weak_ptr;
using std::vector;
using std::map;
using std::string;

namespace zb {

	typedef shared_ptr<tcp::socket> socket_ptr;
	typedef std::map<std::string,std::string> config_type;
	typedef std::vector<config_type> chain_config_type;

	class ZbConfig {
	public:
		enum {
			DEBUG_TUNNEL=1,
			DEBUG_CONNECTION=2,
			DEBUG_SOCKET=4,
			DEBUG_SHADOW=8,
			DEBUG_HTTP=16,
			DEBUG_SOCKS=32,
			DEBUG_CODER=64,
			DEBUG_ALL=0xff
		};

		enum log_level_type {
			LOG_DEBUG=0,
			LOG_WARN=1,
			LOG_NONE=2,
		};

		void log_filter(uint8_t i) {log_filter_ = i;}
		int log_filter() {return log_filter_;}

		void log_level(log_level_type i) {log_level_ = i;}
		log_level_type log_level() {return log_level_;}

		void allow_reuse(bool b) {allow_reuse_ = b;}
		bool allow_reuse() {return allow_reuse_;}

		void log(int f, log_level_type l, string module, string msg) {log_(f, l, module, msg);}

		//static void log(uint8_t f, log_level_type l, string module, string msg) {ZbConfig::get()->log(f, l, module, msg);}

		static ZbConfig *get() {
			if (!ZbConfig::instance_)
				ZbConfig::instance_ = new ZbConfig();
			return ZbConfig::instance_;
		}

	protected:
		int log_filter_;
		bool allow_reuse_;
		log_level_type log_level_;
		boost::function<void (int, log_level_type, string, string)> log_;
		static ZbConfig *instance_;

		ZbConfig() {
			log_level_ = LOG_WARN;
			log_filter_ = DEBUG_TUNNEL | DEBUG_CONNECTION;
			allow_reuse_ = 1;
			log_ = boost::bind(&ZbConfig::_dummy_log, this, _1, _2, _3, _4);
		}

		void _dummy_log(int filter, log_level_type level, string& module, string& msg) {
			if (level < log_level_) return;
			if (level < LOG_WARN && (filter & log_filter_) == 0) return;
			static char* str[3] = {"DEBUG", "WARN", "NONE"};
			std::cout << module + "[" + str[level] + "]: " + msg + "\n";
		};
	};

	typedef ZbConfig gconf_type;
};

#define VERSION "1.0"
#define gconf (*zb::ZbConfig::get())
#define CONFIG_GET(conf, name, _default) conf.count(name) ? conf[name] : _default
#define CONFIG_GET_INT(conf, name, _default) conf.count(name) ? boost::lexical_cast<int>(conf[name]) : _default
#define THROW(str) throw string(str)	
#define STATETHROW(str)	_state_throw(str)
