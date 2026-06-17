/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Exercises the opt-in csv_sink and json_sink: build a few result_t records,
 * write them into std::ostringstream, and assert the output is well-formed
 * (stable CSV header, valid JSON envelope, correct row/object count, and
 * name quoting/escaping for embedded delimiters/quotes).
 */
#include <bench/sink/csv.hpp>
#include <bench/sink/json.hpp>

#include <cstring>
#include <sstream>
#include <string>

namespace {

	bench::result_t make(const char* name, std::uint64_t x){
		bench::result_t r{};
		r.warmup = 2;
		r.sample = 5;
		r.x = x;
		r.mean_runtime    = 1.5;
		r.std_runtime     = 0.25;
		r.mean_throughput = 100.0;
		r.std_throughput  = 3.5;
		std::strncpy(r.name, name, bench::result_t::max_name - 1);
		return r;
	}

	std::size_t count(const std::string& hay, const std::string& needle){
		std::size_t n = 0;
		for(std::size_t p = hay.find(needle); p != std::string::npos;
				p = hay.find(needle, p + needle.size())) ++n;
		return n;
	}
}

int main(){
	// Names with a comma and a double quote to prove CSV quoting and JSON escaping.
	const bench::result_t a = make("read,heavy", 1000);
	const bench::result_t b = make("write \"hot\"", 2000);

	// ---- CSV ----------------------------------------------------------------
	{
		std::ostringstream os;
		bench::csv_sink csv(os);
		csv.write(a);
		csv.write(b);
		csv.flush();
		const std::string out = os.str();

		// stable header on the first line
		if(out.rfind("name,warmup,sample,x,"
				"mean_runtime,std_runtime,mean_throughput,std_throughput\n", 0) != 0)
			return 1;
		// header + 2 data rows => 3 newlines
		if(count(out, "\n") != 3) return 2;
		// comma-bearing name is quoted as a single field
		if(out.find("\"read,heavy\"") == std::string::npos) return 3;
		// embedded double quote is doubled per RFC-4180
		if(out.find("\"write \"\"hot\"\"\"") == std::string::npos) return 4;
		// a numeric field made it through
		if(out.find(",1000,") == std::string::npos) return 5;
	}

	// ---- JSON ---------------------------------------------------------------
	{
		std::ostringstream os;
		{
			bench::json_sink js(os);
			js.write(a);
			js.write(b);
		} // flush on destruction
		const std::string out = os.str();

		if(out.empty() || out.front() != '[' || out.back() != ']') return 6;
		// two objects
		if(count(out, "{\"name\":") != 2) return 7;
		// exactly one separating comma between the two objects
		if(count(out, "},{") != 1) return 8;
		// JSON-escaped double quote
		if(out.find("write \\\"hot\\\"") == std::string::npos) return 9;
		// comma inside the name is preserved verbatim (not a structural comma)
		if(out.find("read,heavy") == std::string::npos) return 10;
		// all fields present
		for(const char* k : {"\"warmup\":", "\"sample\":", "\"x\":",
				"\"mean_runtime\":", "\"std_runtime\":",
				"\"mean_throughput\":", "\"std_throughput\":"}){
			if(count(out, k) != 2) return 11;
		}
	}

	// ---- idempotent flush: second flush must not double-emit ----------------
	{
		std::ostringstream os;
		bench::json_sink js(os);
		js.write(a);
		js.flush();
		const std::string first = os.str();
		js.flush();
		if(os.str() != first) return 12;
	}

	return 0;
}
