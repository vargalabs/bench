/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Exercises the GR.jl-style line/scatter drivers and Solarized theming. Renders
 * into in-memory streams and asserts well-formed SVG (<svg ... </svg>), the
 * presence of the expected primitives (<polyline> for line, <circle> for
 * scatter), and that the active theme's background colour (Solarized dark
 * #002B36) is painted when that theme is selected — both via the global default
 * and via a per-call plot::use{...} override.
 */
#include <plot/all>

#include <sstream>
#include <string>
#include <vector>

namespace {
	bool well_formed(const std::string& s){
		return s.rfind("<svg", 0) == 0 && s.find("</svg>") != std::string::npos;
	}
	std::size_t count(const std::string& hay, const std::string& needle){
		std::size_t n = 0;
		for(std::size_t p = hay.find(needle); p != std::string::npos;
				p = hay.find(needle, p + needle.size())) ++n;
		return n;
	}
}

int main(){
	const std::string DARK_BG  = "#002B36";  // solarized base03 (uppercase hex)
	const std::string LIGHT_BG = "#FDF6E3";  // solarized base3

	std::vector<double> xs{ 1, 2, 3, 4, 5 };
	std::vector<double> ys{ 2, 1, 4, 3, 6 };

	// ---- single-series line, global default = solarized dark ----------------
	plot::theme(plot::solarized_dark);
	{
		std::ostringstream os;
		plot::line(os, xs, ys, plot::title("line"), plot::width{400}, plot::height{260});
		const std::string out = os.str();
		if(!well_formed(out)) return 1;
		if(out.find("<polyline") == std::string::npos) return 2;
		if(out.find(DARK_BG) == std::string::npos) return 3;   // theme bg painted
	}

	// ---- multi-series line with legend --------------------------------------
	{
		std::ostringstream os;
		plot::line(os, xs,
			{ { "a", std::vector<double>{1,2,3,4,5} },
			  { "b", std::vector<double>{5,4,3,2,1} } },
			plot::title("multi"), plot::width{420}, plot::height{280});
		const std::string out = os.str();
		if(!well_formed(out)) return 4;
		if(count(out, "<polyline") != 2) return 5;             // one per series
		if(out.find(DARK_BG) == std::string::npos) return 6;
	}

	// ---- scatter ------------------------------------------------------------
	{
		std::ostringstream os;
		plot::scatter(os, xs, ys, plot::title("scatter"),
				plot::width{400}, plot::height{260});
		const std::string out = os.str();
		if(!well_formed(out)) return 7;
		if(out.find("<circle") == std::string::npos) return 8;
		if(out.find(DARK_BG) == std::string::npos) return 9;
	}

	// ---- per-call theme override (global stays dark, this call light) --------
	{
		std::ostringstream os;
		plot::line(os, xs, ys, plot::use{ plot::solarized_light },
				plot::width{400}, plot::height{260});
		const std::string out = os.str();
		if(!well_formed(out)) return 10;
		if(out.find(LIGHT_BG) == std::string::npos) return 11;  // override wins
		if(out.find(DARK_BG)  != std::string::npos) return 12;  // not the default
	}

	// ---- global default honoured after override (still dark) -----------------
	{
		std::ostringstream os;
		plot::scatter(os, xs, ys, plot::width{300}, plot::height{200});
		const std::string out = os.str();
		if(out.find(DARK_BG) == std::string::npos) return 13;
	}

	return 0;
}
