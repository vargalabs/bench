/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Exercises the opt-in svg_sink: feed a tiny pivoted grid of result rows, render
 * to an in-memory stream, and assert the output is a well-formed standalone SVG
 * (starts "<svg", ends "</svg>") carrying at least one <rect> (a heatmap cell)
 * and the per-cell value <title>s. Also checks the file-backed path emits.
 */
#include <bench/sink/svg.hpp>

#include <cstring>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <string>

namespace {
	bench::result_t mk(const char* name, std::uint64_t x, double mt){
		bench::result_t r{};
		r.warmup = 2; r.sample = 5; r.x = x;
		r.mean_runtime = 1.0; r.std_runtime = 0.1;
		r.mean_throughput = mt; r.std_throughput = mt * 0.1;
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
	// ---- in-memory render of a 2 type x 2 size grid -------------------------
	{
		std::ostringstream os;
		bench::svg_sink svg(os);
		svg.write(mk("scan/int",    1000, 10.0));
		svg.write(mk("scan/int",    2000, 20.0));
		svg.write(mk("scan/double", 1000, 15.0));
		svg.write(mk("scan/double", 2000, 30.0));
		svg.flush();
		const std::string out = os.str();

		if(out.empty()) return 1;
		// well-formed standalone svg
		if(out.rfind("<svg", 0) != 0) return 2;
		if(out.find("</svg>") == std::string::npos) return 3;
		// at least one heatmap cell
		if(out.find("<rect") == std::string::npos) return 4;
		// 4 cells -> 4 cell rects (+1 theme background rect), each cell has a value title
		if(count(out, "<rect") != 5) return 5;
		if(count(out, "<title>") != 4) return 6;
		// axis tick labels present (size axis + type axis)
		if(out.find(">1000<") == std::string::npos) return 7;
		if(out.find(">int<")  == std::string::npos) return 8;
		// idempotent flush
		const std::string first = out;
		svg.flush();
		if(os.str() != first) return 9;
	}

	// ---- 1-D sweep (no '/') collapses onto a single row, still valid --------
	{
		std::ostringstream os;
		bench::svg_sink svg(os);
		svg.write(mk("plain", 1000, 1.0));
		svg.write(mk("plain", 2000, 2.0));
		svg.flush();
		const std::string out = os.str();
		if(out.rfind("<svg", 0) != 0) return 10;
		if(out.find("</svg>") == std::string::npos) return 11;
		// 2 cell rects + 1 theme background rect
		if(count(out, "<rect") != 3) return 12;
	}

	// ---- file-backed path emits a non-empty, well-formed document -----------
	{
		const std::string path = "svg_sink_test.svg";
		{
			bench::svg_sink svg(path);
			svg.write(mk("t/a", 1, 1.0));
			svg.write(mk("t/b", 1, 2.0));
		} // flush on destruction
		std::ifstream in(path, std::ios::binary);
		if(!in) return 13;
		std::string out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		in.close();
		std::remove(path.c_str());
		if(out.rfind("<svg", 0) != 0) return 14;
		if(out.find("</svg>") == std::string::npos) return 15;
		if(out.find("<rect") == std::string::npos) return 16;
	}

	return 0;
}
