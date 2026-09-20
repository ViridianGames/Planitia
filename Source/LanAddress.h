#ifndef _PLANITIA_LANADDRESS_H_
#define _PLANITIA_LANADDRESS_H_

#include <string>
#include <vector>

// Platform networking only — do not include raylib in this TU.
// (Windows CloseWindow / ShowCursor / Rectangle collide with raylib.)
std::vector<std::string> EnumerateLocalIPv4();

#endif
