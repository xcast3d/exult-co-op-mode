/*
 *  Quest of the Companions' own version (qotcversion.h).
 */

#include "qotcversion.h"

#include <ostream>

#if __has_include("qotcbuild.h")
#	include "qotcbuild.h"
#endif
#ifndef QOTC_BUILD
#	define QOTC_BUILD 0    // A build without the makefiles' count (e.g. Visual Studio).
#endif

int QOTC_build() {
	return QOTC_BUILD;
}

std::string QOTC_version_line() {
	std::string line = QOTC_NAME " " QOTC_VERSION;
	if (QOTC_build() > 0) {
		line += " (build " + std::to_string(QOTC_build()) + ")";
	}
	return line;
}

void QOTC_write_version(std::ostream& out) {
	out << QOTC_version_line() << '\n';
}
