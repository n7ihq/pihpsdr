/* Copyright (C)
* 2023 - Christoph van Wüllen, DL1YCF
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

#ifndef __APPLE__
//
// strlcat and strlcpy are the "better replacements" for strncat and strncpy.
// However, they are not in Linux glibc and not POSIX standardized.
// Here we implement without a return value.
//
// If the lenght "len" is zero, both function will quickly return without
// writing anything into memory.
//
// If the destination contains already len characters without beging null-terminated,
// strlcat will return without writing anything into the memory.
//
// In all other cases, it is guaranteed that the destination is null-terminated.
//
// 
//
#include <string.h>

void strlcat(char *dest, const char *src, size_t len) {

  while (len > 0 &&  *dest != '\0') { len--; dest++; }

  if (len == 0) { return; }  // no space in dest

  // cp points to the trailing 0 of dest string

  while (*src != '\0') {
    // copy one byte, but leave at least 1 byte for the
    // null termination
    if (len == 1) { break; }
    *dest++ = *src++;
    len--;
  }

  // null-terminate destination
  *dest = '\0';
}

void strlcpy(char *dest, const char *src, size_t len) {

  if (len == 0) { return; }

  while (len > 1 && *src != '\0') {
    // copy one byte
    *dest++ = *src++;
    len--;
  }

  *dest = '\0';
}
#endif
