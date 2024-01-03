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

#include <gtk/gtk.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "property.h"
#include "message.h"

PROPERTY* properties = NULL;

static double version = 0.0;

void clearProperties() {
  t_print("clearProperties\n");

  if (properties != NULL) {
    // free all the properties
    PROPERTY *next;

    while (properties != NULL) {
      next = properties->next_property;
      free(properties);
      properties = next;
    }
  }
}

/* --------------------------------------------------------------------------*/
/**
* @brief Load Properties
*
* @param filename
*/
void loadProperties(const char* filename) {
  FILE* f = fopen(filename, "r");
  PROPERTY* property;
  t_print("loadProperties: %s\n", filename);
  clearProperties();

  if (f) {
    const char* value;
    const char* name;
    char string[256];

    while (fgets(string, sizeof(string), f)) {
      if (string[0] != '#') {
        name = strtok(string, "=");
        value = strtok(NULL, "\n");

        // Beware of "illegal" lines in corrupted files
        if (name != NULL && value != NULL) {
          property = malloc(sizeof(PROPERTY));
          property->name = g_strdup(name);
          property->value = g_strdup(value);
          property->next_property = properties;
          properties = property;

          if (strcmp(name, "property_version") == 0) {
            version = atof(value);
          }
        }
      }
    }

    fclose(f);
  }

  if (version != PROPERTY_VERSION) {
    properties = NULL;
    t_print("loadProperties: version=%f expected version=%f ignoring\n", version, PROPERTY_VERSION);
  }
}

/* --------------------------------------------------------------------------*/
/**
* @brief Save Properties
*
* @param filename
*/
void saveProperties(const char* filename) {
  PROPERTY* property;
  FILE* f = fopen(filename, "w+");
  char line[512];
  t_print("saveProperties: %s\n", filename);

  if (!f) {
    t_print("can't open %s\n", filename);
    return;
  }

  snprintf(line, 512, "%0.2f", PROPERTY_VERSION);
  setProperty("property_version", line);
  property = properties;

  while (property) {
    snprintf(line, 512, "%s=%s\n", property->name, property->value);
    fwrite(line, 1, strlen(line), f);
    property = property->next_property;
  }

  fclose(f);
}

/* --------------------------------------------------------------------------*/
/**
* @brief Get Properties
*
* @param name
*
* @return
*/
char* getProperty(const char* name) {
  char* value = NULL;
  PROPERTY* property = properties;

  while (property) {
    if (strcmp(name, property->name) == 0) {
      value = property->value;
      break;
    }

    property = property->next_property;
  }

  return value;
}

/* --------------------------------------------------------------------------*/
/**
* @brief Set Properties
*
* @param name
* @param value
*/
void setProperty(const char* name, const char* value) {
  PROPERTY* property = properties;

  while (property) {
    if (strcmp(name, property->name) == 0) {
      break;
    }

    property = property->next_property;
  }

  if (property) {
    // just update
    free(property->value);
    property->value = g_strdup(value);
  } else {
    // new property
    property = malloc(sizeof(PROPERTY));
    property->name = g_strdup(name);
    property->value = g_strdup(value);
    property->next_property = properties;
    properties = property;
  }
}

