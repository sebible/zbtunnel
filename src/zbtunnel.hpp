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

#include "zbconnection.hpp"
#include "zbconnectionmanager.hpp"

namespace zb {

	class ZbTunnel:
		public boost::enable_shared_from_this<ZbTunnel>
	{
		friend class ZbConnection;

	public:
		typedef shared_ptr<ZbTunnel> pointer;

		ZbTunnel(string name);
		ZbTunnel(string name, shared_ptr<io_service>& io_service);
		~ZbTunnel();

		void start_with_config(config_type& config) throw (string);
		void start_with_config(const ptree::ptree& confige) throw (string);

		void start() {running_=true; _start();};
		void stop();
		void _stop_impl();
		virtual void start_with_config(chain_config_type& config) throw (string) {throw string("not implemented");};
		virtual void _start() {throw string("not implemented");};
		virtual void _stop() {throw string("not implemented");};

		void wait();
		void start_worker();

		string last_error() {return last_error_;};
		shared_ptr<boost::thread> get_worker() {return worker_;};

		boost::function<void ()> get_worker_func() {
			return boost::bind(&ZbTunnel::worker, shared_from_this());
		};

		ZB_GETTER_SETTER(running, bool);
		ZB_GETTER_SETTER(name, string);

	protected:
		void init();
		virtual void _init() {throw string("not implmented");};

		void worker();
		void init_coders();
		
		string last_error_;
		string name_;

		bool running_;
		chain_config_type config_;
		ZbConnectionManager::pointer manager_;
		shared_ptr<boost::thread> worker_;
		shared_ptr<io_service> io_service_;
		shared_ptr<tcp::endpoint> endpoint_cache_;
	};

	/////////////////////////////////////
	class ZbSocketTunnel: public ZbTunnel
	{
	public:
		ZbSocketTunnel(string name);
		ZbSocketTunnel(string name, shared_ptr<io_service>& io_service);
		~ZbSocketTunnel();

		string local_address() {return acceptor_.get() != 0 ? acceptor_->local_endpoint().address().to_string() : local_address_;};
		int local_port() {return acceptor_.get() != 0 ? acceptor_->local_endpoint().port() : local_port_;};

		virtual void start_with_config(chain_config_type& config) throw (string);
		virtual void _start();
		virtual void _stop();

	protected:
		virtual void _init();
		void start_accept();
		template <typename SocketTransportPointer>
		void handle_accept(SocketTransportPointer& in,  const error_code& error);

	private:
		typedef scoped_ptr<tcp::acceptor> acceptor_ptr;

		int local_port_, old_local_port_;
		string local_address_, old_local_address_;

		acceptor_ptr acceptor_;
	};

	///////////////////////////////////////
	class ZbIoTunnel: public ZbTunnel
	{
	public:
		ZbIoTunnel(string name);
		ZbIoTunnel(string name, shared_ptr<io_service>& io_service);

		virtual void start_with_config(chain_config_type& config) throw (string);
		virtual void _start();
		virtual void _stop();

	protected:
		virtual void _init();
	};
}
