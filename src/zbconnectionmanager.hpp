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

#pragma once

#include "zbconfig.hpp"
#include "zbtunnel.hpp"

namespace zb {

	class ZbConnectionManager
	{
	public:
		static ZbConnectionManager *get_instance() {
			if (ZbConnectionManager::instance == 0)
				ZbConnectionManager::instance = new ZbConnectionManager;
			return ZbConnectionManager::instance;
		}

		void add(ZbConnection::pointer conn) {
			conns_.insert(conn);
		}

		void remove(ZbConnection::pointer conn) {
			conns_.erase(conn);
			BOOST_FOREACH(named_set::value_type& node, reusable_conns_) {
				node.second.erase(conn);
			};
		}

		void stopAll() {
			BOOST_FOREACH(std::set<ZbConnection::pointer>::value_type node, conns_) {
				node->stop(false);
			}

			BOOST_FOREACH(named_set::value_type& node, reusable_conns_) {
				BOOST_FOREACH(std::set<ZbConnection::pointer>::value_type node2, node.second) {
					node2->stop(false);
				}
			};
		}

		void recycle(ZbConnection::pointer conn) {
			shared_ptr<ZbTunnel> c = conn->client_.lock();
			if (c.get() != 0) {
				string name = c->name();
				if (reusable_conns_.count(name) == 0) {
					reusable_conns_[name] = conn_set();
				}
				reusable_conns_[name].insert(conn);
			}
			conns_.erase(conn);
		}

		ZbConnection::pointer get_or_create_conn(string name, shared_ptr<io_service>& service, ZbConnection::client_ptr client) {
			ZbConnection::pointer p;
			if (reusable_conns_.count(name) > 0 && reusable_conns_[name].size() > 0) {
				p = *(reusable_conns_[name].begin());
				reusable_conns_[name].erase(p);
			} else {
				p = ZbConnection::create(service, client);
				conns_.insert(p);
			}
			return p;
		}

	protected:
		ZbConnectionManager() {};

		static ZbConnectionManager *instance;
		typedef std::set<ZbConnection::pointer> conn_set;
		typedef std::map<string, std::set<ZbConnection::pointer> > named_set;
		conn_set conns_;
		named_set reusable_conns_;
	};

	ZbConnectionManager *ZbConnectionManager::instance = 0;
}