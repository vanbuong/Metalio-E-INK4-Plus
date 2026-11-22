// FatFS stand-in for host tests: text_encoding.cc only needs the OEM code page mapper.
#pragma once

#include <cstdint>

typedef uint16_t WCHAR;

inline WCHAR ff_oem2uni(WCHAR oem, uint16_t /*cp*/) { return oem < 0x80 ? oem : 0; }
