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

namespace zb {
	namespace tunnel {

		/************************
		* Base class of coders
		**/
		class ZbCoder
		{
		protected:
			ZbCoder(string method, string key):method_(method), key_(key) {};

			string method_, key_;

		public:
			virtual void encrypt(uint8_t *src, uint8_t *dst, int length) = 0;
			virtual void decrypt(uint8_t *src, uint8_t *dst, int length) = 0;

			string method() {return method_;};
			string key() {return key_;};
		};

		class ZbCoderPool 
		{
		public:
			typedef struct _key_type{
				string method, key;
				_key_type(string m, string k):method(m), key(k){};
				bool operator < (const _key_type& k2) const{
					return (this->method.compare(k2.method) + this->key.compare(k2.key)) < 0;
				};
			} key_type;
			typedef shared_ptr<ZbCoder> coder_type;

			static ZbCoderPool* get_instance();
			coder_type get_coder(string method, string key) throw (string);

		private:
			explicit ZbCoderPool() {};

			static ZbCoderPool* instance_;
			typedef map<key_type, coder_type> pool_type;
			pool_type pool_;
		};

		class ZbTableCoder: public ZbCoder {
		public:
			ZbTableCoder(string method, string key);
			~ZbTableCoder();

			uint8_t *get_encrypt_table() {return enc_table_;}
			uint8_t *get_decrypt_table() {return dec_table_;}

			virtual void encrypt(uint8_t *src, uint8_t *dst, int length);
			virtual void decrypt(uint8_t *src, uint8_t *dst, int length);

		protected:
			void wait_for_worker();
			void make_table(string key);
			void merge_sort(uint8_t *start, int length, uint64_t keynum, int round);
			void merge(uint8_t *left, int llength, uint8_t *right, int rlength, uint64_t keynum, int round);

			enum {TABLESIZE = 256};
			bool initialized_;
			uint8_t *enc_table_, *dec_table_;
			scoped_ptr<boost::thread> coder_worker_;
		};
	}
}