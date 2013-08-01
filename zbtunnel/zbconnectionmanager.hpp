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

#include "zbtunnel/zbconfig.hpp"
#include "zbtunnel/zbconnection.hpp"

namespace zb {
	namespace tunnel {

		class ZbTunnel;

		class ZbConnectionManager
		{
		public:
			typedef shared_ptr<ZbConnectionManager> pointer;

			ZbConnectionManager(string name_prefix) {
				name_ = name_prefix;
				max_reuse_ = 0;
				preconnect_ = 0;
				recycle_ = false;
				id_ = 0;
			};

			ZB_GETTER_SETTER(max_reuse, int);
			ZB_GETTER_SETTER(preconnect, int);
			ZB_GETTER_SETTER(recycle, bool);

			void add(ZbConnection::pointer conn) {
				conns_.insert(conn);
				gconf.log(gconf_type::DEBUG_CONNECTION_MANAGER, gconf_type::ZBLOG_DEBUG, "ZbConnectionManager", conn->to_string() + string(" added."));
			}

			void remove(ZbConnection::pointer conn) {
				conns_.erase(conn);
				reusable_conns_.erase(conn);
				gconf.log(gconf_type::DEBUG_CONNECTION_MANAGER, gconf_type::ZBLOG_DEBUG, "ZbConnectionManager", conn->to_string() + string(" removed."));
			}

			void stop_all() {
				gconf.log(gconf_type::DEBUG_CONNECTION_MANAGER, gconf_type::ZBLOG_INFO, "ZbConnectionManager", string("Force stop all connections"));

				ZbConnection::pointer p;
				while (conns_.size()) {
					p = (*conns_.begin());
					if (p.get() != 0) p->stop(false, false);
					conns_.erase(p);
				}

				while (reusable_conns_.size()){
					p = (*reusable_conns_.begin());
					if (p.get() != 0) p->stop(false, false);
					reusable_conns_.erase(p);
				}
			}

			bool recycle(ZbConnection::pointer conn) {
				if (!recycle_ || reusable_conns_.size() >= max_reuse_) {
					gconf.log(gconf_type::DEBUG_CONNECTION_MANAGER, gconf_type::ZBLOG_INFO, "ZbConnectionManager", conn->to_string() + string(" recycled rejected."));
					return false;
				}

				reusable_conns_.insert(conn);
				conns_.erase(conn);
				return true;
			}

			ZbConnection::pointer get_or_create_conn(shared_ptr<io_service>& service, ZbConnection::client_ptr client) {
				ZbConnection::pointer p, q;
				if (reusable_conns_.size() > 0) {
					p = *(reusable_conns_.begin());
					reusable_conns_.erase(p);
					conns_.insert(p);
				} else {
					p = ZbConnection::create(service, client);
					p->id(id_++);
					p->owner(name_);
					conns_.insert(p);

					int i = preconnect_;
					while (i-- > 0 && reusable_conns_.size() < max_reuse_) {
						q = ZbConnection::create(service, client);
						q->id(id_++);
						q->owner(name_);
						q->start();
						reusable_conns_.insert(q);
						gconf.log(gconf_type::DEBUG_CONNECTION_MANAGER, gconf_type::ZBLOG_DEBUG, "ZbConnectionManager", q->to_string() + string(" is created for preconnecting."));
					}
				}
				return p;
			}

		protected:
			typedef std::set<ZbConnection::pointer> conn_set;

			string name_;
			unsigned int preconnect_, max_reuse_, id_;
			bool recycle_;
			conn_set conns_;
			conn_set reusable_conns_;
		};
	}
}