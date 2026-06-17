/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Opt-in CSV result sink. RFC-4180-ish: a stable header row followed by one
 * row per result. The `name` field is quoted/escaped. Header-only and
 * dependency-free — include <bench/sink/csv.hpp> explicitly (NOT in <bench/all>).
 */
#ifndef BENCH_SINK_CSV_HPP
#define BENCH_SINK_CSV_HPP

#include <ostream>
#include <string>
#include "../bench.hpp"

namespace bench {

	// Writes results as CSV to a referenced std::ostream. The caller owns the
	// stream's lifetime; this sink only borrows it.
	struct csv_sink : sink {
		explicit csv_sink(std::ostream& os) : os_(os) {}

		void write(const result_t& r) override {
			if(!header_){
				os_ << "name,warmup,sample,x,"
					   "mean_runtime,std_runtime,"
					   "mean_throughput,std_throughput\n";
				header_ = true;
			}
			quote(r.name);
			os_ << ','
				<< r.warmup << ','
				<< r.sample << ','
				<< r.x << ','
				<< r.mean_runtime << ','
				<< r.std_runtime << ','
				<< r.mean_throughput << ','
				<< r.std_throughput << '\n';
		}

		void flush() override { os_.flush(); }

		private:
			// RFC-4180: wrap in double quotes, escape embedded quotes by doubling.
			void quote(const char* s){
				os_ << '"';
				for(; *s; ++s){
					if(*s == '"') os_ << '"';
					os_ << *s;
				}
				os_ << '"';
			}
			std::ostream& os_;
			bool header_ = false;
	};
}

#endif
