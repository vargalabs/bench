/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Opt-in JSON result sink. Buffers result rows and emits a single valid JSON
 * array (one object per result) on flush() / destruction. The `name` string is
 * escaped per JSON rules. Header-only and dependency-free — include
 * <bench/sink/json.hpp> explicitly (NOT in <bench/all>).
 */
#ifndef BENCH_SINK_JSON_HPP
#define BENCH_SINK_JSON_HPP

#include <ostream>
#include <vector>
#include "../bench.hpp"

namespace bench {

	// Buffers results, then writes one JSON array on flush() or at destruction.
	// The caller owns the stream's lifetime; this sink only borrows it.
	struct json_sink : sink {
		explicit json_sink(std::ostream& os) : os_(os) {}
		~json_sink() override { flush(); }

		void write(const result_t& r) override { rows_.push_back(r); }

		void flush() override {
			if(emitted_) return;
			emitted_ = true;
			os_ << '[';
			for(std::size_t i = 0; i < rows_.size(); ++i){
				const result_t& r = rows_[i];
				if(i) os_ << ',';
				os_ << "{\"name\":\"";
				escape(r.name);
				os_ << "\",\"warmup\":"        << r.warmup
					<< ",\"sample\":"          << r.sample
					<< ",\"x\":"               << r.x
					<< ",\"mean_runtime\":"    << r.mean_runtime
					<< ",\"std_runtime\":"     << r.std_runtime
					<< ",\"mean_throughput\":" << r.mean_throughput
					<< ",\"std_throughput\":"  << r.std_throughput
					<< '}';
			}
			os_ << ']';
			os_.flush();
		}

		private:
			void escape(const char* s){
				for(; *s; ++s){
					const unsigned char c = static_cast<unsigned char>(*s);
					switch(c){
						case '"':  os_ << "\\\""; break;
						case '\\': os_ << "\\\\"; break;
						case '\b': os_ << "\\b";  break;
						case '\f': os_ << "\\f";  break;
						case '\n': os_ << "\\n";  break;
						case '\r': os_ << "\\r";  break;
						case '\t': os_ << "\\t";  break;
						default:
							if(c < 0x20){
								static const char hex[] = "0123456789abcdef";
								os_ << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
							} else {
								os_ << static_cast<char>(c);
							}
					}
				}
			}
			std::ostream& os_;
			std::vector<result_t> rows_;
			bool emitted_ = false;
	};
}

#endif
