/*  minIni - Multi-Platform INI file parser, suitable for embedded systems
 *  Optimized for the PlayStation: Portable
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

/* Make it Read-Only if needed */
/* #define INI_READONLY */

#include <psptypes.h>
#include <string.h>
#include "minGlue.h"

#if !defined INI_BUFFERSIZE
  #define INI_BUFFERSIZE  512
#endif

/* force INI_LINETERM to be '\n' */
#if !defined INI_LINETERM
  #define INI_LINETERM                  "\n"
  #define INI_LINETERMCHAR              '\n'
#endif

#if !defined strnicmp
  #define strnicmp strncasecmp
#endif

SceBool   ini_getbool(const char *Section, const char *Key, SceBool DefValue, const char *Filename);
SceInt32  ini_geti(const char *Section, const char *Key, SceInt32 DefValue, const char *Filename);
SceInt32  ini_gets(const char *Section, const char *Key, const char *DefValue, char *Buffer, int BufferSize, const char *Filename);
SceInt32  ini_getsection(int idx, char *Buffer, int BufferSize, const char *Filename);
SceInt32  ini_getkey(const char *Section, int idx, char *Buffer, int BufferSize, const char *Filename);
SceFloat  ini_getf(const char *Section, const char *Key, SceFloat DefValue, const char *Filename);

SceBool   ini_hassection(const char *Section, const char *Filename);
SceBool   ini_haskey(const char *Section, const char *Key, const char *Filename);

#if !defined INI_READONLY
SceBool  ini_putbool(const char *Section, const char *Key, SceBool Value, const char *Filename);
SceBool  ini_puti(const char *Section, const char *Key, SceInt32 Value, const char *Filename);
SceBool  ini_puts(const char *Section, const char *Key, const char *Value, const char *Filename);
SceBool  ini_putf(const char *Section, const char *Key, SceFloat Value, const char *Filename);
#endif /* INI_READONLY */

#if !defined INI_NOBROWSE
typedef SceBool (*INI_CALLBACK)(const char *Section, const char *Key, const char *Value, void *UserData);
SceBool  ini_browse(INI_CALLBACK Callback, void *UserData, const char *Filename);
#endif /* INI_NOBROWSE */

#endif /* MININI_H */
