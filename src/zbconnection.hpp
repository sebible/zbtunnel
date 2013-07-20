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

namespace zb {

	class ZbTunnel;
	class ZbConnectionManager;
	class ZbTransport;

	class ZbConnection
	  : public boost::enable_shared_from_this<ZbConnection>
	{
		friend class ZbConnectionManager;
	public:
		enum {BUFSIZE = 8192};
		typedef shared_ptr<ZbConnection> pointer;
		typedef weak_ptr<ZbTunnel> client_ptr;
		typedef uint8_t buf_type[2][BUFSIZE]; // 0 for read, 1 for write
		
		static pointer create(shared_ptr<io_service>& io_service, client_ptr client);

		string to_string();
		template <typename TransportPointer>
		void start(TransportPointer in);
		void start();
		void stop(bool recycle, bool remove = true);

		ZB_GETTER_SETTER(id, int);
		ZB_GETTER_SETTER(owner, string);

	private:
		ZbConnection();
		void handle_connect(const error_code& error);
		void handle_init(const error_code& error);
		void handle_transfer(const error_code& error, size_t size, int direction);
		void handle_write(const error_code& error, size_t bytes_transferred, int direction);
		string _state_throw(string msg);

		int current_, id_;
		string owner_;
		buf_type buf_[2]; // two directions
		client_ptr client_;
		shared_ptr<ZbTransport> in_, out_;
		enum {INIT, CONNECTING, CONNECTED, BAD} state_;
	};

}