#ifndef UTILS_HPP
#define UTILS_HPP

#include <iomanip>
#include <ostream>

struct _FormatMillis {
	long millis;
};

inline _FormatMillis formatMillis(long millis) {
	return { millis };
}

template<typename _CharT, typename _Traits> inline std::basic_ostream<_CharT, _Traits>& 
	operator<<(std::basic_ostream<_CharT, _Traits>& os, _FormatMillis millis) { 
		float val = millis.millis / 1000;
		const char *unit = "seconds";

		if (val > 60) {
			val /= 60;
			unit = val > 1 ? "minutes" : "minute";
		}

		if (val > 90) {
			val /= 60;
			unit = val > 1 ? "hours" : "hour";
		}

		return os << std::fixed << std::setprecision(1) << val << " " << unit;
	}

#endif