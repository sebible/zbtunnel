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
#include "zbcoder.hpp"
#include "base64.h"

namespace zb {

	class ZbTransport
	{
	public:
		typedef boost::function<void (const error_code&)> connect_handler_type;
		typedef boost::function<void (const error_code&, const size_t)> read_handler_type;
		typedef boost::function<void (const error_code&, const size_t)> write_handler_type;
		typedef boost::function<void ()> callback_type;
		typedef shared_ptr<ZbTransport> pointer;
		typedef uint8_t* data_type;
		typedef ZbTransport lowest_layer_type;

		ZbTransport(const pointer& parent):parent_(parent) {
			if (parent_.get() && parent->io_service_.get() != 0)
				io_service_ = parent->io_service_;
		};

		io_service& get_io_service() {
			assert(io_service_.get() != 0);
			return *io_service_;
		}

		lowest_layer_type& lowest_layer() {
			return *this;
		}

		string last_error() {
			return last_error_;
		}

		virtual bool is_open() {
			if (parent_.get() != 0) 
				return parent_->is_open();

			return last_error_.empty();
		}

		virtual void async_connect(string host, string port, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {};
		virtual void async_connect(const tcp::endpoint& endpoint, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			async_connect(endpoint.address().to_string(), boost::lexical_cast<string>(endpoint.port()), handler);
		}
		virtual void handle_read_data(data_type data, size_t size) {};
		virtual void handle_write_data(data_type data, size_t size) {};

		virtual void close() {
			if (parent_.get() != 0) parent_->close();
		};

		virtual void init(BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			invoke_callback(boost::bind(handler, error_code()));
		}

		virtual void async_send(const data_type data,const size_t size,
			BOOST_ASIO_MOVE_ARG(write_handler_type) handler)
		{
			handle_write_data(data, size);
			assert(parent_.get() != 0);
			parent_->async_send(data, size, BOOST_ASIO_MOVE_CAST(write_handler_type)(handler));
		};

		virtual void async_write_some(const boost::asio::mutable_buffer& buffer,
			BOOST_ASIO_MOVE_ARG(write_handler_type) handler) {
			std::size_t size = boost::asio::buffer_size(buffer);
			data_type data = boost::asio::buffer_cast<data_type>(buffer);
			async_send(data, size, handler);
		}

		virtual void async_receive(const data_type& data, const size_t& size,
			BOOST_ASIO_MOVE_ARG(read_handler_type) handler)
		{
			assert(parent_.get() != 0);
			read_handler_type r = boost::bind(&ZbTransport::_read_handler, this, _1, _2, data, handler);
			parent_->async_receive(data, size, r);
		}

		virtual void async_read_some(const boost::asio::mutable_buffer& buffer,
			BOOST_ASIO_MOVE_ARG(read_handler_type) handler)
		{
			std::size_t size = boost::asio::buffer_size(buffer);
			data_type data = boost::asio::buffer_cast<data_type>(buffer);
			async_receive(data, size, handler);
		}

		void _read_handler(const error_code& error, const size_t size, const data_type data, 
			BOOST_ASIO_MOVE_ARG(read_handler_type) handler)
		{
			handle_read_data(data, size);
			invoke_callback(boost::bind(handler, error, size));		
		}

		void _dummy_write_handler(const error_code& error, const size_t size) {}

		virtual void invoke_callback(callback_type func) {
			assert(io_service_.get() != 0);
			io_service_->post(func);
		}


	protected:
		string last_error_;
		pointer parent_;
		shared_ptr<io_service> io_service_;
	};

	/////////////////////////////////////
	class ZbSocketTransport: public ZbTransport
	{
	protected:
		socket_ptr socket_;
		scoped_ptr<tcp::resolver> resolver_;

	public:
		typedef shared_ptr<ZbSocketTransport> pointer;

		ZbSocketTransport(const socket_ptr& socket, shared_ptr<io_service> service):ZbTransport(ZbTransport::pointer()), socket_(socket) {
			io_service_ = service;
		};

		const tcp::endpoint get_endpoint() {
			assert(socket_.get() != 0);
			return socket_->remote_endpoint();
		}

		socket_ptr socket() {
			assert(socket_.get() != 0);
			return socket_;
		}

		virtual bool is_open() {
			assert(socket_.get() != 0);
			return socket_->is_open();
		}

		virtual void close() {
			assert(socket_.get() != 0);
			socket_->close();
		}

		virtual void async_connect(string host, string port, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			resolver_.reset(new tcp::resolver(*io_service_));
			tcp::resolver::query socks_query(host, port, tcp::resolver::query::all_matching | tcp::resolver::query::numeric_service);


			gconf.log(gconf_type::DEBUG_SOCKS, gconf_type::LOG_DEBUG, "ZbSocketTransport", string("Resolving ") + host + ":" + port);
			resolver_->async_resolve(socks_query, boost::bind(&ZbSocketTransport::_handle_resolve, this, _1, _2, handler));
		};

		void _handle_resolve(const error_code& error, tcp::resolver::iterator iterator, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			if (error) {
				last_error_ = error.message();
				invoke_callback(boost::bind(handler, error));
				return;
			}

			gconf.log(gconf_type::DEBUG_SOCKS, gconf_type::LOG_DEBUG, "ZbSocketTransport", "Resolved");
			assert(socket_.get() != 0);
			boost::asio::async_connect(*socket_, iterator, boost::bind(handler, _1));
		}

		virtual void async_connect(const tcp::endpoint& endpoint, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			socket_->async_connect(endpoint, handler);
		}

		virtual void async_send(const data_type data,const size_t size,
			BOOST_ASIO_MOVE_ARG(write_handler_type) handler)
		{
			gconf.log(gconf_type::DEBUG_SOCKS, gconf_type::LOG_DEBUG, "ZbSocketTransport", string("Sending:\n") + string((char*)data, size));
			socket_->async_send(boost::asio::buffer(data, size), handler);
		};

		virtual void async_receive(const data_type& data, const size_t& size,
			BOOST_ASIO_MOVE_ARG(read_handler_type) handler)
		{
			socket_->async_read_some(boost::asio::buffer(data, size), handler);
		};
	};

	class ZbShadowTransport: public ZbTransport
	{
	protected:
		ZbCoderPool::coder_type coder_;
		string method, key;
		uint8_t buf[256];

	public:
		ZbShadowTransport(pointer& parent, config_type& conf):ZbTransport(parent) {
			method = CONFIG_GET(conf, "method", "");
			key = CONFIG_GET(conf, "key", THROW("shadow key missing"));

			ZbCoderPool* cp = ZbCoderPool::get_instance();
			assert(cp != 0);

			coder_ = cp->get_coder(method, key);
		}

		virtual void async_connect(string host, string port, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			if (host.empty() || port.empty()) {
				last_error_ = string("Bad host:port");
				invoke_callback(boost::bind(handler, make_error_code(errc::bad_address)));
			}

			std::stringstream s;
			int port_ = boost::lexical_cast<int>(port);
			uint8_t h = (uint8_t)(port_ >> 8), l = (uint8_t)(port_ & 0xff);
			s << "\x03"  << (char)host.size() << host <<  h << l;
			string s2 = s.str();
			memcpy(buf, s2.data(), s2.size() < sizeof(buf) ? s2.size() : sizeof(buf));
			async_send((const data_type)(buf), s2.size(), boost::bind(&ZbTransport::_dummy_write_handler, this, _1, _2));

			invoke_callback(boost::bind(handler, error_code()));
		};

		virtual void handle_read_data(data_type data, size_t size) {
			assert(coder_.get() != 0);
			coder_->decrypt(data, data, size);
			gconf.log(gconf_type::DEBUG_SHADOW, gconf_type::LOG_DEBUG, "ZbShadowTransport", string("decoded: ") + string((char*)data, size));
		};

		virtual void handle_write_data(data_type data, size_t size) {
			assert(coder_.get() != 0);
			gconf.log(gconf_type::DEBUG_SHADOW, gconf_type::LOG_DEBUG, "ZbShadowTransport", string("encoded: ") + string((char*)data, size));
			coder_->encrypt(data, data, size);
		};
	}; // ZbShadowTransport

	class ZbHttpTransport: public ZbTransport
	{
	protected:
		uint8_t buf[256];
		size_t pos;
		string username, password;
		string connect_string;

	public:
		ZbHttpTransport(pointer& parent, config_type& conf):ZbTransport(parent), pos(0) {
			username = CONFIG_GET(conf, "username", "");
			password = CONFIG_GET(conf, "password", "");
		}

		virtual void async_connect(string host, string port, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			if (host.empty() || port.empty()) {
				last_error_ = string("Bad host:port");
				invoke_callback(boost::bind(handler, make_error_code(errc::bad_address)));
			}

			std::stringstream s;
			s << "CONNECT " << host << ":" << port << " HTTP/1.1\r\n";
			//s << "Host: " << host << ":" << port << "\r\n";
			s << "User-Agent: " << UA_STRING << "\r\n";
			if (!username.empty()) {
				string tmp = username + ":" + password;
				s << "Proxy-Authorization: basic " << base64_encode((const unsigned char*)tmp.c_str(), tmp.size()) << "\r\n";
			}
			s << "\r\n";
			connect_string = s.str();
			gconf.log(gconf_type::DEBUG_HTTP, gconf_type::LOG_DEBUG, "ZbHttpTransport", string("connecting\n") + connect_string);
			async_send((const data_type)(connect_string.c_str()), connect_string.size(), boost::bind(&ZbTransport::_dummy_write_handler, this, _1, _2));
			async_receive(buf, sizeof(buf), boost::bind(&ZbHttpTransport::_handle_http_connect, static_cast<ZbHttpTransport*>(this), _1, _2, handler));
		};

		void _handle_http_connect(const error_code& error, const size_t size, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			if (error && size <= 0) {
				last_error_ = error.message();
				invoke_callback(boost::bind(handler, error));
				return;
			}

			pos += size;
			if (pos > 4 && string((char*)(buf + pos - 4), 4).compare("\r\n\r\n") == 0) {
				gconf.log(gconf_type::DEBUG_HTTP, gconf_type::LOG_DEBUG, "ZbHttpTransport", string("received\n") + string((char*)buf, pos));
				char code = *(buf + 9);
				gconf.log(gconf_type::DEBUG_HTTP, gconf_type::LOG_DEBUG, "ZbHttpTransport", string("result code:") + string((char*)buf + 9, 3));
				if (code == '2') {
					invoke_callback(boost::bind(handler, error));
				} else {
					string s((char*)buf, pos);
					size_t tmp = s.find("\r\n");
					last_error_ = s.substr(13, tmp - 13);
					invoke_callback(boost::bind(handler, make_error_code(errc::permission_denied)));
				}
			} 
			else if (pos >= sizeof(buf)) {
				last_error_ = "ZbHttpTransport receive buffer runs out.";
				invoke_callback(boost::bind(handler, make_error_code(errc::no_buffer_space)));
			}
			else {
				async_receive(buf + pos, sizeof(buf) - pos, boost::bind(&ZbHttpTransport::_handle_http_connect, static_cast<ZbHttpTransport*>(this), _1, _2, handler));
			}
		}
	}; // ZbHttpTransport

#ifdef WITH_OPENSSL
	class ZbHttpsTransport: public ZbHttpTransport
	{
	protected:
		typedef boost::asio::ssl::stream<ZbTransport> stream_type;
		typedef shared_ptr<stream_type> stream_ptr;
		stream_ptr stream_;
		scoped_ptr<boost::asio::ssl::context> ctx;

	public:
		ZbHttpsTransport(pointer& parent, config_type& conf):ZbHttpTransport(parent, conf) {
			string ssl_type = CONFIG_GET(conf, "ssl_type", "sslv23");
			if (ssl_type.compare("sslv23") == 0)
				ctx.reset(new boost::asio::ssl::context(boost::asio::ssl::context::sslv23));
			else if (ssl_type.compare("tls1") == 0)
				ctx.reset(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
			stream_.reset(new stream_type(*this, *ctx));
		}

		virtual void init(BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			assert(stream_.get() != 0);
			stream_->async_handshake(boost::asio::ssl::stream_base::client, handler);
		}

		virtual void async_send(const data_type data,const size_t size,
			BOOST_ASIO_MOVE_ARG(write_handler_type) handler)
		{
			stream_->async_write_some(boost::asio::buffer(data, size), handler);
		};

		virtual void async_receive(const data_type& data, const size_t& size,
			BOOST_ASIO_MOVE_ARG(read_handler_type) handler)
		{
			stream_->async_read_some(boost::asio::buffer(data, size), handler);
		};
	}; // ZbHttpsTransport
#endif // WITH_OPENSSL

	class ZbSocks5Transport: public ZbTransport
	{
	protected:
		string username, password;
		uint8_t req[64];
		uint8_t buf[64];
		size_t pos;
		enum {INIT, GREETING, AUTH, STANDBY, CONNECTING, CONNECTED} state;

	public:
		ZbSocks5Transport(pointer& parent, config_type& conf):ZbTransport(parent),state(INIT), pos(0) {
			username = CONFIG_GET(conf, "username", "");
			password = CONFIG_GET(conf, "password", "");
		}

		virtual void init(BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			std::stringstream s;
			size_t l = 0;
			if (username.empty()) {
				memcpy(req, "\x05\x01\x00", 3);
				l = 3;
			} else {
				memcpy(req, "\x05\x02\x00\x02", 4);
				l = 4;
			}
			state = GREETING;
			async_send(req, l, boost::bind(&ZbTransport::_dummy_write_handler, this, _1, _2));
			pos = 0;
			async_receive(buf, sizeof(buf), boost::bind(&ZbSocks5Transport::_handle_socks, static_cast<ZbSocks5Transport*>(this), _1, _2, 2, handler));
		}

		void _handle_socks(const error_code& error, const size_t size, const size_t target_size, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			if (error) {
				last_error_ = error.message();
				invoke_callback(boost::bind(handler, error));
				return;
			}

			pos += size;
			if (pos < target_size) {
				// Not enough bytes
				async_receive(buf + pos, sizeof(buf) - pos, boost::bind(&ZbSocks5Transport::_handle_socks, static_cast<ZbSocks5Transport*>(this), _1, _2, target_size, handler));
				return;
			}

			if (state == GREETING) {
				if (buf[0] != 5) {
					last_error_ = string("Client uses version 5. But server requires version ") + boost::lexical_cast<string>(buf[0]);
					invoke_callback(boost::bind(handler, make_error_code(errc::protocol_not_supported)));
				}

				if (buf[1] == 0xff) {
					last_error_ = string("Server doesn't support our authentication method.");
					invoke_callback(boost::bind(handler, make_error_code(errc::protocol_not_supported)));
				} 
				else if (buf[1] == 2) {
					// Auth
					state = AUTH;
					std::stringstream s;
					s << "\x01" << (char)username.size() << username << (char)password.size() << password;
					string t = s.str();
					memcpy(req, t.c_str(), t.size());
					async_send(req, t.size(), boost::bind(&ZbTransport::_dummy_write_handler, this, _1, _2));
					pos = 0;
					async_receive(buf, sizeof(buf), boost::bind(&ZbSocks5Transport::_handle_socks, static_cast<ZbSocks5Transport*>(this), _1, _2, 2, handler));
				} else {
					// Done
					state = STANDBY;
					invoke_callback(boost::bind(handler, error_code()));
				}
			} else if (state == AUTH) {
				if (buf[1] == 0) {
					// Done
					state = STANDBY;
					invoke_callback(boost::bind(handler, error_code()));
				} else {
					last_error_ = string("Authentication failed.");
					invoke_callback(boost::bind(handler, make_error_code(errc::permission_denied)));
				}
			} else if (state == CONNECTING) {
				if (buf[2] == 0) {
					// Consume remaining bytes
					size_t tsize = 4;
					if (buf[4] == 1) tsize += 4 + 2;
					if (buf[4] == 4) tsize += 16 + 2;
					if (buf[4] == 3) tsize += 1 + buf[5] + 2;
					if (pos < tsize) {
						async_receive(buf + pos, sizeof(buf) - pos, boost::bind(&ZbSocks5Transport::_handle_socks, static_cast<ZbSocks5Transport*>(this), _1, _2, tsize, handler));
					}

					// Connected
					state = CONNECTED;
					invoke_callback(boost::bind(handler, error_code()));
				}
			}
		}

		virtual void async_connect(string host, string port, BOOST_ASIO_MOVE_ARG(connect_handler_type) handler) {
			if (host.empty() || port.empty()) {
				last_error_ = string("Bad host:port");
				invoke_callback(boost::bind(handler, make_error_code(errc::bad_address)));
			}

			if (state != STANDBY) {
				last_error_ = string("Handshake not succeeded yet.");
				invoke_callback(boost::bind(handler, make_error_code(errc::operation_in_progress)));
			}

			std::stringstream s;
			int port_ = boost::lexical_cast<int>(port);
			uint8_t h = (uint8_t)(port_ >> 8), l = (uint8_t)(port_ & 0xff);
			s << (char)host.size() << host << h << l;
			string tmp = s.str();
			memcpy(req, "\x05\x01\x00\x03", 4);
			memcpy(req + 4, tmp.c_str(), tmp.size());

			state = CONNECTING;
			async_send((const data_type)req, tmp.size() + 4, boost::bind(&ZbTransport::_dummy_write_handler, this, _1, _2));
			// target to receive at least 6 bytes which will at least include the starting of address
			pos = 0;
			async_receive(buf, sizeof(buf), boost::bind(&ZbSocks5Transport::_handle_socks, static_cast<ZbSocks5Transport*>(this), _1, _2, 6, handler));
		}
	};
}
