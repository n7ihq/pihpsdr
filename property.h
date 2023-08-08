/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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

#ifndef _PROPERTY_H
#define _PROPERTY_H

#define PROPERTY_VERSION 3.0

typedef struct _PROPERTY PROPERTY;

struct _PROPERTY {
  char* name;
  char* value;
  PROPERTY* next_property;
};

extern void clearProperties(void);
extern void loadProperties(char* filename);
extern char* getProperty(char* name);
extern void setProperty(char* name, char* value);
extern void saveProperties(char* filename);

//
// Some macros to get/set properties. It is assumed that variables
// "value" and "name" are used in the conventional way.
// The macros are define such that replacing "Set" by "Get" at the beginning
// transforms a macro that sets a property by a macro that reads the same
// property.
// This facilitates to keep the save/restore functions sync'ed
//
// The macros are names as follows: zzzPropXn, where
//
//  zzz = "Get" or "Set"
//  X   = I, F, S, A  for integer (long long), double, or string, or action
//  n   = 0, 1, 2, 3  for scalars or quantities with 1...3 indices
//

#define GetPropI0(a,b)  { \
  value=getProperty(a); \
  if (value) { b = atoll(value); } \
}

#define GetPropF0(a,b)  { \
  value=getProperty(a); \
  if (value) { b = atof(value); } \
}

#define GetPropS0(a,b)  { \
  value=getProperty(a); \
  if (value) { strcpy(b, value); } \
}

#define GetPropA0(a,b)  { \
  value=getProperty(a); \
  if (value) { b = String2Action(value); } \
}

#define GetPropI1(a,b,c) { \
  sprintf(name, a, b); \
  value=getProperty(name); \
  if (value) { c = atoll(value); } \
}

#define GetPropF1(a,b,c) { \
  sprintf(name, a, b); \
  value=getProperty(name); \
  if (value) { c = atof(value); } \
}

#define GetPropS1(a,b,c) { \
  sprintf(name, a, b); \
  value=getProperty(name); \
  if (value) { strcpy(c, value); } \
}

#define GetPropA1(a,b,c) { \
  sprintf(name, a, b); \
  value=getProperty(name); \
  if (value) { c = String2Action(value); } \
}

#define GetPropI2(a,b,c,d) { \
  sprintf(name, a, b, c); \
  value=getProperty(name); \
  if (value) { d = atoll(value); } \
}

#define GetPropS2(a,b,c,d) { \
  sprintf(name, a, b, c); \
  value=getProperty(name); \
  if (value) { strcpy(d, value); } \
}

#define GetPropA2(a,b,c,d) { \
  sprintf(name, a, b, c); \
  value=getProperty(name); \
  if (value) { d = String2Action(value); } \
}

#define GetPropI3(a,b,c,d, e) { \
  sprintf(name, a, b, c, d); \
  value=getProperty(name); \
  if (value) { e = atoll(value); } \
}

#define GetPropS3(a,b,c,d, e) { \
  sprintf(name, a, b, c, d); \
  value=getProperty(name); \
  if (value) { strcpy(e, value); } \
}

#define GetPropA3(a,b,c,d,e) { \
  sprintf(name, a, b, c, d); \
  value=getProperty(name); \
  if (value) { e = String2Action(value); } \
}

#define SetPropI0(a,b) { \
  sprintf(value,"%lld", (long long)(b)); \
  setProperty(a, value); \
}

#define SetPropF0(a,b) { \
  sprintf(value,"%f", (double) (b)); \
  setProperty(a, value); \
}

#define SetPropS0(a,b) { \
  setProperty(a, b); \
}

#define SetPropI1(a,b,c) { \
  sprintf(name, a, b); \
  sprintf(value, "%lld", (long long) (c)); \
  setProperty(name, value); \
}

#define SetPropF1(a,b,c) { \
  sprintf(name, a, b); \
  sprintf(value, "%f", (double) (c)); \
  setProperty(name, value); \
}

#define SetPropS1(a,b,c) { \
  sprintf(name, a, b); \
  setProperty(name, c); \
}

#define SetPropA1(a,b,c) { \
  sprintf(name, a, b); \
  Action2String(c, value); \
  setProperty(name, value); \
}

#define SetPropI2(a,b,c,d) { \
  sprintf(name, a, b, c); \
  sprintf(value, "%lld", (long long) (d)); \
  setProperty(name, value); \
}

#define SetPropS2(a,b,c,d) { \
  sprintf(name, a, b, c); \
  setProperty(name, d); \
}

#define SetPropA2(a,b,c,d) { \
  sprintf(name, a, b, c); \
  Action2String(d, value); \
  setProperty(name, value); \
}

#define SetPropI3(a,b,c,d, e) { \
  sprintf(name, a, b, c, d); \
  sprintf(value, "%lld", (long long) (e)); \
  setProperty(name, value); \
}

#define SetPropS3(a,b,c,d, e) { \
  sprintf(name, a, b, c, d); \
  setProperty(name, e); \
}

#define SetPropA3(a,b,c,d,e) { \
  sprintf(name, a, b, c, d); \
  Action2String(e, value); \
  setProperty(name, value); \
}

#endif
