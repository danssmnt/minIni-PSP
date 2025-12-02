/*  minIni - Multi-Platform INI file parser, suitable for embedded systems
 *  pspIni - A optimized fork for the PlayStation: Portable
 *
 *  Copyright (c) CompuPhase, 2008-2024
 *  Copyright (c) danssmnt,   2025
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  Version: $Id: minIni.h 53 2015-01-18 13:35:11Z thiadmer.riemersma@gmail.com $
 */
#ifndef MININI_H
#define MININI_H

#include <psptypes.h>
#include <string.h>

#define INI_FALSE 0
#define INI_TRUE  1

/* Read-Only */
#ifndef INI_READONLY
  #define INI_READONLY  INI_FALSE
#endif

/* INI Browser function */
#ifndef INI_BROWSE
  #define INI_BROWSE    INI_TRUE
#endif

/* INI Debug (for asserts). Only use when debugging this library! */
#ifndef INI_DEBUG
  #define INI_DEBUG     INI_FALSE
#endif

/* Default BufferSize for LocalBuffers */
#ifndef INI_BUFFERSIZE
  #define INI_BUFFERSIZE  512
#endif

/* Default Newline */
#ifndef INI_LINETERM
  #define INI_LINETERM      "\n"
  #define INI_LINETERMCHAR  '\n'
#endif

int       ini_geti(const char *Section, const char *Key, int DefValue, const char *Filename);
SceUInt   ini_getu(const char *Section, const char *Key, SceUInt DefValue, const char *Filename);
SceBool   ini_getbool(const char *Section, const char *Key, SceBool DefValue, const char *Filename);
float     ini_getf(const char *Section, const char *Key, float DefValue, const char *Filename);
SceSize   ini_gets(const char *Section, const char *Key, const char *DefValue, char *Buffer, SceSize BufferSize, const char *Filename);
SceSize   ini_getsection(int idx, char *Buffer, SceSize BufferSize, const char *Filename);
SceSize   ini_getkey(const char *Section, int idx, char *Buffer, SceSize BufferSize, const char *Filename);

SceBool   ini_hassection(const char *Section, const char *Filename);
SceBool   ini_haskey(const char *Section, const char *Key, const char *Filename);

#if !INI_READONLY
SceBool   ini_puti(const char *Section, const char *Key, int Value, const char *Filename);
SceBool   ini_putu(const char *Section, const char *Key, SceUInt Value, const char *Filename);
SceBool   ini_putbool(const char *Section, const char *Key, SceBool Value, const char *Filename);
SceBool   ini_putf(const char *Section, const char *Key, float Value, const char *Filename);
SceBool   ini_puts(const char *Section, const char *Key, const char *Value, const char *Filename);
#endif /* INI_READONLY */

#if INI_BROWSE
typedef SceBool (*INI_CALLBACK)(const char *Section, const char *Key, const char *Value, void *UserData);
SceBool   ini_browse(INI_CALLBACK Callback, void *UserData, const char *Filename);
#endif /* INI_BROWSE */

#endif /* MININI_H */