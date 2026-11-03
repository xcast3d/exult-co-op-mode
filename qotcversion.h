#ifndef QOTCVERSION_H
#define QOTCVERSION_H

/*
 *  Quest of the Companions (QOTC): this project's own version, kept apart
 *  from Exult's (VERSION, version.cc), which stays as upstream has it.
 *  CHANGELOG.md says how the version goes up; the build number is the
 *  count of commits since we branched from Exult (generated into
 *  qotcbuild.h by the makefiles; 0 where it can't be counted).
 */

#include <iosfwd>
#include <string>

#define QOTC_NAME    "Quest of the Companions"
#define QOTC_VERSION "0.8.2"

// The build number (0 if unknown).
int QOTC_build();

// "Quest of the Companions 0.8.1 (build 207)".
std::string QOTC_version_line();

// That line, for --version and the version files saved with a game, ahead
// of Exult's own (getVersionInfo).
void QOTC_write_version(std::ostream& out);

#endif
