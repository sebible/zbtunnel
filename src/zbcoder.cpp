/* 
* Part of code is from shadowsocks-libev https://github.com/clowwindy/shadowsocks-libev.git 
* And since that project is under GPL. So this file is under GPL. Full GPL text is included.
*/

#include "zbcoder.hpp"
#include "md5.h"
#include <boost/integer.hpp>


#define OFFSET_ROL(p, o) ((uint64_t)(*(p + o)) << (8 * o))

namespace zb {

	ZbCoderPool* ZbCoderPool::instance_ = 0;

	ZbCoderPool* ZbCoderPool::get_instance() {
		if (ZbCoderPool::instance_ == 0)
			ZbCoderPool::instance_ = new ZbCoderPool();
		return ZbCoderPool::instance_;
	}

	ZbCoderPool::coder_type ZbCoderPool::get_coder(string method, string key) throw (string){
		key_type k(method, key);
		pool_type::iterator iter = pool_.find(k);
		if (iter != pool_.end())
			return (*iter).second;

		if (key.empty())
			throw string("empty key");

		if (method.empty() || method.compare("shadow") == 0) {
			ZbTableCoder *c = new ZbTableCoder(method, key);
			pool_[k] = shared_ptr<ZbCoder>(c);
			return pool_[k];
		}

		throw string("unsupported");
	}

	// Shadow table coder
	ZbTableCoder::ZbTableCoder(string method, string key):ZbCoder(method, key), enc_table_(0), dec_table_(0), initialized_(false) {
		if (method.empty()) {
			if (key.empty()) {
				throw string("empty key");
			}

			coder_worker_.reset(new boost::thread(boost::bind(&ZbTableCoder::make_table, this, key)));
		} else {
			throw string("unsupported");
		}
	}

	ZbTableCoder::~ZbTableCoder() {
		if (enc_table_) free(enc_table_);
		if (dec_table_) free(dec_table_);
	}

	void ZbTableCoder::make_table(string key) {
		gconf.log(gconf_type::DEBUG_CODER, gconf_type::LOG_DEBUG, "ZbTableCoder", "Initializing");
		if (!enc_table_) enc_table_ = (uint8_t*)malloc(TABLESIZE);
		if (!dec_table_) dec_table_ = (uint8_t*)malloc(TABLESIZE);

		uint8_t *table = enc_table_;
		uint64_t keynum[2];
		int i = 0;

		MD5 md5(key);
		md5.finalize();
		unsigned char *tmp = md5.get_digest();
		memcpy(keynum, tmp, 8);

		for(i = 0; i < 256; ++i) {
			table[i] = i;
		}

		for(i = 1; i < 1024; ++i) {
			// Use only first 8 bytes of the hash
			merge_sort(table, TABLESIZE, keynum[0], i);
		}

		for(i = 0; i < 256; ++i) {
			dec_table_[enc_table_[i]] = i;
		}

		initialized_ = true;
		gconf.log(gconf_type::DEBUG_CODER, gconf_type::LOG_DEBUG, "ZbTableCoder", "Initialized");
	}

	void ZbTableCoder::wait_for_worker() {
		if (coder_worker_.get() != 0 && !initialized_)
			coder_worker_->join();
	}

	void ZbTableCoder::encrypt(uint8_t *src, uint8_t *dst, int length) {
		wait_for_worker();

		if (method_.empty() && enc_table_) {
			while (length > 0) {
				*dst = enc_table_[*src];
				dst++;
				src++;
				length--;
			}
		}
	}

	void ZbTableCoder::decrypt(uint8_t *src, uint8_t *dst, int length) {
		wait_for_worker();

		if (method_.empty() && dec_table_) {
			while (length > 0) {
				*dst = dec_table_[*src];
				dst++;
				src++;
				length--;
			}
		}
	}

	void ZbTableCoder::merge_sort(uint8_t *data, int length, uint64_t keynum, int round) {
		/* This is the middle index and also the length of the right array. */
		uint8_t middle;

		/*
		 * Pointers to the beginning of the left and right segment of the array
		 * to be merged.
		 */
		uint8_t *left, *right;

		/* Length of the left segment of the array to be merged. */
		int llength;

		if (length <= 1)
			return;

		/* Let integer division truncate the value. */
		middle = length / 2;

		llength = length - middle;

		/*
		 * Set the pointers to the appropriate segments of the array to be merged.
		 */
		left = data;
		right = data + llength;

		merge_sort(left, llength, keynum, round);
		merge_sort(right, middle, keynum, round);
		merge(left, llength, right, middle, keynum, round);
	}

	void ZbTableCoder::merge(uint8_t *left, int llength, uint8_t *right, int rlength, uint64_t keynum, int round)
	{
		/* Temporary memory locations for the 2 segments of the array to merge. */
		uint8_t *ltmp = (uint8_t *) malloc(llength * sizeof(uint8_t));
		uint8_t *rtmp = (uint8_t *) malloc(rlength * sizeof(uint8_t));

		/*
		 * Pointers to the elements being sorted in the temporary memory locations.
		 */
		uint8_t *ll = ltmp;
		uint8_t *rr = rtmp;

		uint8_t *result = left;

		/*
		 * Copy the segment of the array to be merged into the temporary memory
		 * locations.
		 */
		memcpy(ltmp, left, llength * sizeof(uint8_t));
		memcpy(rtmp, right, rlength * sizeof(uint8_t));

		while (llength > 0 && rlength > 0) {
			if (keynum % (*ll + round) <= keynum % (*rr + round)) {
				/*
				 * Merge the first element from the left back into the main array
				 * if it is smaller or equal to the one on the right.
				 */
				*result = *ll;
				++ll;
				--llength;
			} else {
				/*
				 * Merge the first element from the right back into the main array
				 * if it is smaller than the one on the left.
				 */
				*result = *rr;
				++rr;
				--rlength;
			}
			++result;
		}
		/*
		 * All the elements from either the left or the right temporary array
		 * segment have been merged back into the main array.  Append the remaining
		 * elements from the other temporary array back into the main array.
		 */
		if (llength > 0)
			while (llength > 0) {
				/* Appending the rest of the left temporary array. */
				*result = *ll;
				++result;
				++ll;
				--llength;
			}
		else
			while (rlength > 0) {
				/* Appending the rest of the right temporary array. */
				*result = *rr;
				++result;
				++rr;
				--rlength;
			}

		/* Release the memory used for the temporary arrays. */
		free(ltmp);
		free(rtmp);
	}

}
