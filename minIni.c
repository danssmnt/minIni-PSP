/*  minIni - Multi-Platform INI file parser, suitable for embedded systems
 *  Optimized for the PlayStation: Portable
 *
 *  These routines are in part based on the article "Multiplatform .INI Files"
 *  by Joseph J. Graf in the March 1994 issue of Dr. Dobb's Journal.
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
 *  Version: $Id: minIni.c 53 2015-01-18 13:35:11Z thiadmer.riemersma@gmail.com $
 */

#define MININI_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

#include "minIni.h"
#if defined NDEBUG
  #define assert(e)
#else
  #include <assert.h>
#endif

#if !defined sizearray
  #define sizearray(a)    (sizeof(a) / sizeof((a)[0]))
#endif

enum quote_option {
  QUOTE_NONE,
  QUOTE_ENQUOTE,
  QUOTE_DEQUOTE,
};

/* Mimic fgets behavior with PSPSDK functions
 * Returns the equivalent to fgets
 * Thx go to Freakler for his code: https://github.com/Freakler/CheatDeviceRemastered/blob/d537e30f6fb927cc873e5756c7a4afe07c267c93/source/minIni.c#L96
 */
SceBool psp_read_fgets(char *s, SceSize n, INI_FILETYPE *stream)
{
  if ( n == 0 || s == NULL || stream == NULL ) return 0;

  int bytes_read = sceIoRead(*stream, s, n - 1);

  /* If nothing was read or it errored out, fgets returns NULL */
  if (bytes_read <= 0) return 0;

  /* Read until newline (or until string end if newline isn't found) */
  int i = 0;
  while(i < bytes_read)
  {
    if( s[i++] == INI_LINETERMCHAR ) break;
  }

  s[i] = '\0';

  if (bytes_read > i)
    sceIoLseek32(*stream, -(bytes_read - i), PSP_SEEK_CUR);

  return 1;
}

static char *skipleading(const char *str)
{
  assert(str != NULL);
  while ('\0' < *str && *str <= ' ')
    str++;
  return (char *)str;
}

static char *skiptrailing(const char *str, const char *base)
{
  assert(str != NULL);
  assert(base != NULL);
  while (str > base && '\0' < *(str-1) && *(str-1) <= ' ')
    str--;
  return (char *)str;
}

static char *striptrailing(char *str)
{
  char *ptr = skiptrailing(strchr(str, '\0'), str);
  assert(ptr != NULL);
  *ptr = '\0';
  return str;
}

static char *ini_strncpy(char *dest, const char *source, size_t maxlen, enum quote_option option)
{
  size_t d, s;

  assert(maxlen>0);
  assert(source != NULL && dest != NULL);
  assert((dest < source || (dest == source && option != QUOTE_ENQUOTE)) || dest > source + strlen(source));
  if (option == QUOTE_ENQUOTE && maxlen < 3)
    option = QUOTE_NONE;  /* cannot store two quotes and a terminating zero in less than 3 characters */

  switch (option) {
  case QUOTE_NONE:
    for (d = 0; d < maxlen - 1 && source[d] != '\0'; d++)
      dest[d] = source[d];
    assert(d < maxlen);
    dest[d] = '\0';
    break;
  case QUOTE_ENQUOTE:
    d = 0;
    dest[d++] = '"';
    for (s = 0; source[s] != '\0' && d < maxlen - 2; s++, d++) {
      if (source[s] == '"') {
        if (d >= maxlen - 3)
          break;  /* no space to store the escape character plus the one that follows it */
        dest[d++] = '\\';
      }
      dest[d] = source[s];
    }
    dest[d++] = '"';
    dest[d] = '\0';
    break;
  case QUOTE_DEQUOTE:
    for (d = s = 0; source[s] != '\0' && d < maxlen - 1; s++, d++) {
      if ((source[s] == '"' || source[s] == '\\') && source[s + 1] == '"')
        s++;
      dest[d] = source[s];
    }
    dest[d] = '\0';
    break;
  default:
    assert(0);
  }

  return dest;
}

static char *cleanstring(char *string, enum quote_option *quotes)
{
  int isstring;
  char *ep;

  assert(string != NULL);
  assert(quotes != NULL);

  /* Remove a trailing comment */
  isstring = 0;
  for (ep = string; *ep != '\0' && ((*ep != ';' && *ep != '#') || isstring); ep++) {
    if (*ep == '"') {
      if (*(ep + 1) == '"')
        ep++;                 /* skip "" (both quotes) */
      else
        isstring = !isstring; /* single quote, toggle isstring */
    } else if (*ep == '\\' && *(ep + 1) == '"') {
      ep++;                   /* skip \" (both quotes */
    }
  }
  assert(ep != NULL && (*ep == '\0' || *ep == ';' || *ep == '#'));
  *ep = '\0';                 /* terminate at a comment */
  striptrailing(string);
  /* Remove double quotes surrounding a value */
  *quotes = QUOTE_NONE;
  if (*string == '"' && (ep = strchr(string, '\0')) != NULL && *(ep - 1) == '"') {
    string++;
    *--ep = '\0';
    *quotes = QUOTE_DEQUOTE;  /* this is a string, so remove escaped characters */
  }
  return string;
}

static SceBool getkeystring(INI_FILETYPE *fp, const char *Section, const char *Key,
                        int idxSection, int idxKey, char *Buffer, int BufferSize,
                        INI_FILEPOS *mark)
{
  char *sp, *ep;
  int len, idx;
  enum quote_option quotes;
  char LocalBuffer[INI_BUFFERSIZE];

  assert(fp != NULL);
  /* Move through file 1 line at a time until a section is matched or EOF. If
   * parameter Section is NULL, only look at keys above the first section. If
   * idxSection is positive, copy the relevant section name.
   */
  len = (Section != NULL) ? (int)strlen(Section) : 0;
  if (len > 0 || idxSection >= 0) {
    assert(idxSection >= 0 || Section != NULL);
    idx = -1;
    do {
      do {
        if (!ini_read(LocalBuffer, INI_BUFFERSIZE, fp))
          return 0;
        sp = skipleading(LocalBuffer);
        ep = strrchr(sp, ']');
      } while (*sp != '[' || ep == NULL);
      /* When arrived here, a section was found; now optionally skip leading and
       * trailing whitespace.
       */
      assert(sp != NULL && *sp == '[');
      sp = skipleading(sp + 1);
      assert(ep != NULL && *ep == ']');
      ep = skiptrailing(ep, sp);
    } while ((((int)(ep-sp) != len || Section == NULL || strnicmp(sp, Section, len) != 0) && ++idx != idxSection));
    if (idxSection >= 0) {
      if (idx == idxSection) {
        assert(ep != NULL);
        *ep = '\0'; /* the end of the section name was found earlier */
        ini_strncpy(Buffer, sp, BufferSize, QUOTE_NONE);
        return 1;
      }
      return 0; /* no more section found */
    }
  }

  /* Now that the section has been found, find the entry.
   * Stop searching upon leaving the section's area.
   */
  assert(Key != NULL || idxKey >= 0);
  len = (Key != NULL) ? (int)strlen(Key) : 0;
  idx = -1;
  do {
    if (mark != NULL)
      (void)ini_tell(fp, mark);   /* optionally keep the mark to the start of the line */
    if (!ini_read(LocalBuffer,INI_BUFFERSIZE,fp) || *(sp = skipleading(LocalBuffer)) == '[')
      return 0;
    sp = skipleading(LocalBuffer);
    ep = strchr(sp, '=');  /* Parse out the equal sign */
    if (ep == NULL)
      ep = strchr(sp, ':');
  } while (*sp == ';' || *sp == '#' || ep == NULL
           || ((len == 0 || (int)(skiptrailing(ep,sp)-sp) != len || strnicmp(sp,Key,len) != 0) && ++idx != idxKey));
  if (idxKey >= 0) {
    if (idx == idxKey) {
      assert(ep != NULL);
      assert(*ep == '=' || *ep == ':');
      *ep = '\0';
      striptrailing(sp);
      ini_strncpy(Buffer, sp, BufferSize, QUOTE_NONE);
      return 1;
    }
    return 0;   /* no more key found (in this section) */
  }

  /* Copy up to BufferSize chars to buffer */
  assert(ep != NULL);
  assert(*ep == '=' || *ep == ':');
  sp = skipleading(ep + 1);
  sp = cleanstring(sp, &quotes);  /* Remove a trailing comment */
  ini_strncpy(Buffer, sp, BufferSize, quotes);
  return 1;
}

/** ini_gets()
 * \param Section     the name of the section to search for
 * \param Key         the name of the entry to find the value of
 * \param DefValue    default string in the event of a failed read
 * \param Buffer      a pointer to the buffer to copy into
 * \param BufferSize  the maximum number of characters to copy
 * \param Filename    the name and full path of the .ini file to read from
 *
 * \return            the number of characters copied into the supplied buffer
 */
SceInt32 ini_gets(const char *Section, const char *Key, const char *DefValue,
             char *Buffer, int BufferSize, const char *Filename)
{
  INI_FILETYPE fp;
  int ok = 0;

  if (Buffer == NULL || BufferSize <= 0 || Key == NULL)
    return 0;
  if (ini_openread(Filename, &fp)) {
    ok = getkeystring(&fp, Section, Key, -1, -1, Buffer, BufferSize, NULL);
    (void)ini_close(&fp);
  }
  if (!ok)
    ini_strncpy(Buffer, (DefValue != NULL) ? DefValue : "", BufferSize, QUOTE_NONE);
  return (int)strlen(Buffer);
}

/** ini_geti()
 * \param Section     the name of the section to search for
 * \param Key         the name of the entry to find the value of
 * \param DefValue    the default value in the event of a failed read
 * \param Filename    the name of the .ini file to read from
 *
 * \return            the value located at Key
 */
SceInt32 ini_geti(const char *Section, const char *Key, SceInt32 DefValue, const char *Filename)
{
  char LocalBuffer[64];
  int len = ini_gets(Section, Key, "", LocalBuffer, sizearray(LocalBuffer), Filename);
  return (len == 0) ? DefValue
                    : ((len >= 2 && (LocalBuffer[1] == 'x' || LocalBuffer[1] == 'X')) ? (int)strtol(LocalBuffer, NULL, 16)
                                                                           : (int)strtol(LocalBuffer, NULL, 10));
}

/** ini_getf()
 * \param Section     the name of the section to search for
 * \param Key         the name of the entry to find the value of
 * \param DefValue    the default value in the event of a failed read
 * \param Filename    the name of the .ini file to read from
 *
 * \return            the value located at Key
 */
SceFloat ini_getf(const char *Section, const char *Key, SceFloat DefValue, const char *Filename)
{
  char LocalBuffer[64];
  int len = ini_gets(Section, Key, "", LocalBuffer, sizearray(LocalBuffer), Filename);
  return (len == 0) ? DefValue : ini_atof(LocalBuffer);
}

/** ini_getbool()
 * \param Section     the name of the section to search for
 * \param Key         the name of the entry to find the value of
 * \param DefValue    default value in the event of a failed read; it should
 *                    zero (0) or one (1).
 * \param Filename    the name and full path of the .ini file to read from
 *
 * A true boolean is found if one of the following is matched:
 * - A string starting with 'y' or 'Y'
 * - A string starting with 't' or 'T'
 * - A string starting with '1'
 *
 * A false boolean is found if one of the following is matched:
 * - A string starting with 'n' or 'N'
 * - A string starting with 'f' or 'F'
 * - A string starting with '0'
 *
 * \return            the true/false flag as interpreted at Key
 */
SceBool ini_getbool(const char *Section, const char *Key, SceBool DefValue, const char *Filename)
{
  char LocalBuffer[2] = "";
  int ret;

  ini_gets(Section, Key, "", LocalBuffer, sizearray(LocalBuffer), Filename);
  if (LocalBuffer[0] == 'Y' || LocalBuffer[0] == 'y' || LocalBuffer[0] == '1' || LocalBuffer[0] == 'T' || LocalBuffer[0] == 't')
    ret = 1;
  else if (LocalBuffer[0] == 'N' ||LocalBuffer[0] == 'n' || LocalBuffer[0] == '0' || LocalBuffer[0] == 'F' || LocalBuffer[0] == 'f')
    ret = 0;
  else
    ret = DefValue;

  return(ret);
}

/** ini_getsection()
 * \param idx         the zero-based sequence number of the section to return
 * \param Buffer      a pointer to the buffer to copy into
 * \param BufferSize  the maximum number of characters to copy
 * \param Filename    the name and full path of the .ini file to read from
 *
 * \return            the number of characters copied into the supplied buffer
 */
SceInt32  ini_getsection(int idx, char *Buffer, int BufferSize, const char *Filename)
{
  INI_FILETYPE fp;
  int ok = 0;

  if (Buffer == NULL || BufferSize <= 0 || idx < 0)
    return 0;
  if (ini_openread(Filename, &fp)) {
    ok = getkeystring(&fp, NULL, NULL, idx, -1, Buffer, BufferSize, NULL);
    (void)ini_close(&fp);
  }
  if (!ok)
    *Buffer = '\0';
  return (int)strlen(Buffer);
}

/** ini_getkey()
 * \param Section     the name of the section to browse through, or NULL to
 *                    browse through the keys outside any section
 * \param idx         the zero-based sequence number of the key to return
 * \param Buffer      a pointer to the buffer to copy into
 * \param BufferSize  the maximum number of characters to copy
 * \param Filename    the name and full path of the .ini file to read from
 *
 * \return            the number of characters copied into the supplied buffer
 */
SceInt32  ini_getkey(const char *Section, int idx, char *Buffer, int BufferSize, const char *Filename)
{
  INI_FILETYPE fp;
  int ok = 0;

  if (Buffer == NULL || BufferSize <= 0 || idx < 0)
    return 0;
  if (ini_openread(Filename, &fp)) {
    ok = getkeystring(&fp, Section, NULL, -1, idx, Buffer, BufferSize, NULL);
    (void)ini_close(&fp);
  }
  if (!ok)
    *Buffer = '\0';
  return (int)strlen(Buffer);
}

/** ini_hassection()
 * \param Section     the name of the section to search for
 * \param Filename    the name of the .ini file to read from
 *
 * \return            1 if the section is found, 0 if not found
 */
SceBool ini_hassection(const char *Section, const char *Filename)
{
  char LocalBuffer[32];  /* dummy buffer */
  INI_FILETYPE fp;
  int ok = 0;

  if (ini_openread(Filename, &fp)) {
    ok = getkeystring(&fp, Section, NULL, -1, 0, LocalBuffer, sizearray(LocalBuffer), NULL);
    (void)ini_close(&fp);
  }
  return ok;
}

/** ini_haskey()
 * \param Section     the name of the section to search for
 * \param Key         the name of the entry to find the value of
 * \param Filename    the name of the .ini file to read from
 *
 * \return            1 if the key is found, 0 if not found
 */
SceBool ini_haskey(const char *Section, const char *Key, const char *Filename)
{
  char LocalBuffer[32];  /* dummy buffer */
  INI_FILETYPE fp;
  SceBool ok = 0;

  if (ini_openread(Filename, &fp)) {
    ok = getkeystring(&fp, Section, Key, -1, -1, LocalBuffer, sizearray(LocalBuffer), NULL);
    (void)ini_close(&fp);
  }
  return ok;
}


#if !defined INI_NOBROWSE
/** ini_browse()
 * \param Callback    a pointer to a function that will be called for every
 *                    setting in the INI file.
 * \param UserData    arbitrary data, which the function passes on the
 *                    \c Callback function
 * \param Filename    the name and full path of the .ini file to read from
 *
 * \return            1 on success, 0 on failure (INI file not found)
 *
 * \note              The \c Callback function must return 1 to continue
 *                    browsing through the INI file, or 0 to stop. Even when the
 *                    callback stops the browsing, this function will return 1
 *                    (for success).
 */
SceBool ini_browse(INI_CALLBACK Callback, void *UserData, const char *Filename)
{
  char LocalBuffer[INI_BUFFERSIZE];
  int lenSec, lenKey;
  enum quote_option quotes;
  INI_FILETYPE fp;

  if (Callback == NULL)
    return 0;
  if (!ini_openread(Filename, &fp))
    return 0;

  LocalBuffer[0] = '\0';   /* copy an empty section in the buffer */
  lenSec = (int)strlen(LocalBuffer) + 1;
  for ( ;; ) {
    char *sp, *ep;
    if (!ini_read(LocalBuffer + lenSec, INI_BUFFERSIZE - lenSec, &fp))
      break;
    sp = skipleading(LocalBuffer + lenSec);
    /* ignore empty strings and comments */
    if (*sp == '\0' || *sp == ';' || *sp == '#')
      continue;
    /* see whether we reached a new section */
    ep = strrchr(sp, ']');
    if (*sp == '[' && ep != NULL) {
      sp = skipleading(sp + 1);
      ep = skiptrailing(ep, sp);
      *ep = '\0';
      ini_strncpy(LocalBuffer, sp, INI_BUFFERSIZE, QUOTE_NONE);
      lenSec = (int)strlen(LocalBuffer) + 1;
      continue;
    }
    /* not a new section, test for a key/value pair */
    ep = strchr(sp, '=');    /* test for the equal sign or colon */
    if (ep == NULL)
      ep = strchr(sp, ':');
    if (ep == NULL)
      continue;               /* invalid line, ignore */
    *ep++ = '\0';             /* split the key from the value */
    striptrailing(sp);
    ini_strncpy(LocalBuffer + lenSec, sp, INI_BUFFERSIZE - lenSec, QUOTE_NONE);
    lenKey = (int)strlen(LocalBuffer + lenSec) + 1;
    /* clean up the value */
    sp = skipleading(ep);
    sp = cleanstring(sp, &quotes);  /* Remove a trailing comment */
    ini_strncpy(LocalBuffer + lenSec + lenKey, sp, INI_BUFFERSIZE - lenSec - lenKey, quotes);
    /* call the callback */
    if (!Callback(LocalBuffer, LocalBuffer + lenSec, LocalBuffer + lenSec + lenKey, UserData))
      break;
  }

  (void)ini_close(&fp);
  return 1;
}
#endif /* INI_NOBROWSE */

#if ! defined INI_READONLY
static void ini_tempname(char *dest, const char *source, int maxlength)
{
  char *p;

  ini_strncpy(dest, source, maxlength, QUOTE_NONE);
  p = strchr(dest, '\0');
  assert(p != NULL);
  *(p - 1) = '~';
}

static enum quote_option check_enquote(const char *Value)
{
  const char *p;

  /* run through the value, if it has trailing spaces, or '"', ';' or '#'
   * characters, enquote it
   */
  assert(Value != NULL);
  for (p = Value; *p != '\0' && *p != '"' && *p != ';' && *p != '#'; p++)
    /* nothing */;
  return (*p != '\0' || (p > Value && *(p - 1) == ' ')) ? QUOTE_ENQUOTE : QUOTE_NONE;
}

static void writesection(char *LocalBuffer, SceSize BufferSize, const char *Section, INI_FILETYPE *fp)
{
  if (Section != NULL && strlen(Section) > 0) {
    char *p;
    LocalBuffer[0] = '[';
    ini_strncpy(LocalBuffer + 1, Section, INI_BUFFERSIZE - 3, QUOTE_NONE);  /* -1 for '[', -1 for ']', -1 for '\n' */
    p = strchr(LocalBuffer, '\0');
    assert(p != NULL);
    *p++ = ']';
    strcpy(p, INI_LINETERM); /* copy line terminator (typically "\n") */
    if (fp != NULL)
      (void)ini_write(LocalBuffer, strlen(LocalBuffer), fp);
  }
}

static void writekey(char *LocalBuffer, const char *Key, const char *Value, INI_FILETYPE *fp)
{
  char *p;
  enum quote_option option = check_enquote(Value);
  ini_strncpy(LocalBuffer, Key, INI_BUFFERSIZE - 2, QUOTE_NONE);  /* -1 for '=', -1 for '\n' */
  p = strchr(LocalBuffer, '\0');
  assert(p != NULL);
  *p++ = '=';
  ini_strncpy(p, Value, INI_BUFFERSIZE - (p - LocalBuffer) - 1, option); /* -1 for '\n' */
  p = strchr(LocalBuffer, '\0');
  assert(p != NULL);
  strcpy(p, INI_LINETERM); /* copy line terminator (typically "\n") */
  if (fp != NULL)
    (void)ini_write(LocalBuffer, strlen(LocalBuffer), fp);
}

static int cache_accum(const char *string, int *size, int max)
{
  int len = (int)strlen(string);
  if (*size + len >= max)
    return 0;
  *size += len;
  return 1;
}

static int cache_flush(char *buffer, int *size,
                      INI_FILETYPE *rfp, INI_FILETYPE *wfp, INI_FILEPOS *mark)
{
  int terminator_len = (int)strlen(INI_LINETERM);
  int pos = 0, pos_prev = -1;

  (void)ini_seek(rfp, mark);
  assert(buffer != NULL);
  buffer[0] = '\0';
  assert(size != NULL);
  assert(*size <= INI_BUFFERSIZE);
  while (pos < *size && pos != pos_prev) {
    pos_prev = pos;     /* to guard against zero bytes in the INI file */
    (void)ini_read(buffer + pos, INI_BUFFERSIZE - pos, rfp);
    while (pos < *size && buffer[pos] != '\0')
      pos++;            /* cannot use strlen() because buffer may not be zero-terminated */
  }
  if (buffer[0] != '\0') {
    assert(pos > 0 && pos <= INI_BUFFERSIZE);
    if (pos == INI_BUFFERSIZE)
      pos--;
    buffer[pos] = '\0'; /* force zero-termination (may be left unterminated in the above while loop) */
    (void)ini_write(buffer, *size, wfp);
  }
  (void)ini_tell(rfp, mark);  /* update mark */
  *size = 0;
  /* return whether the buffer ended with a line termination */
  return (pos > terminator_len) && (strcmp(buffer + pos - terminator_len, INI_LINETERM) == 0);
}

static int close_rename(INI_FILETYPE *rfp, INI_FILETYPE *wfp, const char *filename, char *buffer)
{
  (void)ini_close(rfp);
  (void)ini_close(wfp);
  (void)ini_tempname(buffer, filename, INI_BUFFERSIZE);
  #if defined ini_remove || defined INI_REMOVE
    (void)ini_remove(filename);
  #endif
  (void)ini_rename(buffer, filename);
  return 1;
}

/** ini_puts()
 * \param Section     the name of the section to write the string in
 * \param Key         the name of the entry to write, or NULL to erase all keys in the section
 * \param Value       a pointer to the buffer the string, or NULL to erase the key
 * \param Filename    the name and full path of the .ini file to write to
 *
 * \return            1 if successful, otherwise 0
 */
SceBool ini_puts(const char *Section, const char *Key, const char *Value, const char *Filename)
{
  INI_FILETYPE rfp;
  INI_FILETYPE wfp;
  INI_FILEPOS mark;
  INI_FILEPOS head, tail;
  char *sp, *ep;
  char LocalBuffer[INI_BUFFERSIZE];
  int len, match, flag, cachelen;

  assert(Filename != NULL);
  if (!ini_openread(Filename, &rfp)) {
    /* If the .ini file doesn't exist, make a new file */
    if (Key != NULL && Value != NULL) {
      if (!ini_openwrite(Filename, &wfp))
        return 0;
      writesection(LocalBuffer, INI_BUFFERSIZE, Section, &wfp);
      writekey(LocalBuffer, Key, Value, &wfp);
      (void)ini_close(&wfp);
    }
    return 1;
  }

  /* If parameters Key and Value are valid (so this is not an "erase" request)
   * and the setting already exists, there are two short-cuts to avoid rewriting
   * the INI file.
   */
  if (Key != NULL && Value != NULL) {
    match = getkeystring(&rfp, Section, Key, -1, -1, LocalBuffer, sizearray(LocalBuffer), &head);
    if (match) {
      /* if the current setting is identical to the one to write, there is
       * nothing to do.
       */
      if (strcmp(LocalBuffer,Value) == 0) {
        (void)ini_close(&rfp);
        return 1;
      }
      /* if the new setting has the same length as the current setting, and the
       * glue file permits file read/write access, we can modify in place.
       */
      #if defined ini_openrewrite || defined INI_OPENREWRITE
        /* we already have the start of the (raw) line, get the end too */
        (void)ini_tell(&rfp, &tail);
        /* create new buffer (without writing it to file) */
        writekey(LocalBuffer, Key, Value, NULL);
        if (strlen(LocalBuffer) == (size_t)(tail - head)) {
          /* length matches, close the file & re-open for read/write, then
           * write at the correct position
           */
          (void)ini_close(&rfp);
          if (!ini_openrewrite(Filename, &wfp))
            return 0;
          (void)ini_seek(&wfp, &head);
          (void)ini_write(LocalBuffer, strlen(LocalBuffer), &wfp);
          (void)ini_close(&wfp);
          return 1;
        }
      #endif
    }
    /* key not found, or different value & length -> proceed */
  } else if (Key != NULL && Value == NULL) {
    /* Conversely, for a request to delete a setting; if that setting isn't
       present, just return */
    match = getkeystring(&rfp, Section, Key, -1, -1, LocalBuffer, sizearray(LocalBuffer), NULL);
    if (!match) {
      (void)ini_close(&rfp);
      return 1;
    }
    /* key found -> proceed to delete it */
  }

  /* Get a temporary file name to copy to. Use the existing name, but with
   * the last character set to a '~'.
   */
  (void)ini_close(&rfp);
  ini_tempname(LocalBuffer, Filename, INI_BUFFERSIZE);
  if (!ini_openwrite(LocalBuffer, &wfp))
    return 0;
  /* In the case of (advisory) file locks, ini_openwrite() may have been blocked
   * on the open, and after the block is lifted, the original file may have been
   * renamed, which is why the original file was closed and is now reopened */
  if (!ini_openread(Filename, &rfp)) {
    /* If the .ini file doesn't exist any more, make a new file */
    assert(Key != NULL && Value != NULL);
    writesection(LocalBuffer, INI_BUFFERSIZE, Section, &wfp);
    writekey(LocalBuffer, Key, Value, &wfp);
    (void)ini_close(&wfp);
    return 1;
  }

  (void)ini_tell(&rfp, &mark);
  cachelen = 0;

  /* Move through the file one line at a time until a section is
   * matched or until EOF. Copy to temp file as it is read.
   */
  len = (Section != NULL) ? (int)strlen(Section) : 0;
  if (len > 0) {
    do {
      if (!ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp)) {
        /* Failed to find section, so add one to the end */
        flag = cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
        if (Key!=NULL && Value!=NULL) {
          if (!flag)
            (void)ini_write(INI_LINETERM, 1, &wfp);  /* force a new line behind the last line of the INI file */
          writesection(LocalBuffer, INI_BUFFERSIZE, Section, &wfp);
          writekey(LocalBuffer, Key, Value, &wfp);
        }
        return close_rename(&rfp, &wfp, Filename, LocalBuffer);  /* clean up and rename */
      }
      /* Check whether this line is a section */
      sp = skipleading(LocalBuffer);
      ep = strrchr(sp, ']');
      match = (*sp == '[' && ep != NULL);
      if (match) {
        /* A section was found, skip leading and trailing whitespace */
        assert(sp != NULL && *sp == '[');
        sp = skipleading(sp + 1);
        assert(ep != NULL && *ep == ']');
        ep = skiptrailing(ep, sp);
        match = ((int)(ep-sp) == len && strnicmp(sp, Section, len) == 0);
      }
      /* Copy the line from source to dest, but not if this is the section that
       * we are looking for and this section must be removed
       */
      if (!match || Key != NULL) {
        if (!cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE)) {
          cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
          (void)ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp);
          cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE);
        }
      }
    } while (!match);
  }
  cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
  /* when deleting a section, the section head that was just found has not been
   * copied to the output file, but because this line was not "accumulated" in
   * the cache, the position in the input file was reset to the point just
   * before the section; this must now be skipped (again)
   */
  if (Key == NULL) {
    (void)ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp);
    (void)ini_tell(&rfp, &mark);
  }

  /* Now that the section has been found, find the entry. Stop searching
   * upon leaving the section's area. Copy the file as it is read
   * and create an entry if one is not found.
   */
  len = (Key != NULL) ? (int)strlen(Key) : 0;
  for( ;; ) {
    if (!ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp)) {
      /* EOF without an entry so make one */
      flag = cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
      if (Key!=NULL && Value!=NULL) {
        if (!flag)
          (void)ini_write(INI_LINETERM, 1, &wfp);  /* force a new line behind the last line of the INI file */
        writekey(LocalBuffer, Key, Value, &wfp);
      }
      return close_rename(&rfp, &wfp, Filename, LocalBuffer);  /* clean up and rename */
    }
    sp = skipleading(LocalBuffer);
    ep = strchr(sp, '='); /* Parse out the equal sign */
    if (ep == NULL)
      ep = strchr(sp, ':');
    match = (ep != NULL && len > 0 && (int)(skiptrailing(ep,sp)-sp) == len && strnicmp(sp,Key,len) == 0);
    if ((Key != NULL && match) || *sp == '[')
      break;  /* found the key, or found a new section */
    /* copy other keys in the section */
    if (Key == NULL) {
      (void)ini_tell(&rfp, &mark);  /* we are deleting the entire section, so update the read position */
    } else {
      if (!cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE)) {
        cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
        (void)ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp);
        cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE);
      }
    }
  }
  /* the key was found, or we just dropped on the next section (meaning that it
   * wasn't found); in both cases we need to write the key, but in the latter
   * case, we also need to write the line starting the new section after writing
   * the key
   */
  flag = (*sp == '[');
  cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
  if (Key != NULL && Value != NULL)
    writekey(LocalBuffer, Key, Value, &wfp);
  /* cache_flush() reset the "read pointer" to the start of the line with the
   * previous key or the new section; read it again (because writekey() destroyed
   * the buffer)
   */
  (void)ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp);
  if (flag) {
    /* the new section heading needs to be copied to the output file */
    cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE);
  } else {
    /* forget the old key line */
    (void)ini_tell(&rfp, &mark);
  }
  /* Copy the rest of the INI file */
  while (ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp)) {
    if (!cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE)) {
      cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
      (void)ini_read(LocalBuffer, INI_BUFFERSIZE, &rfp);
      cache_accum(LocalBuffer, &cachelen, INI_BUFFERSIZE);
    }
  }
  cache_flush(LocalBuffer, &cachelen, &rfp, &wfp, &mark);
  return close_rename(&rfp, &wfp, Filename, LocalBuffer);  /* clean up and rename */
}

/** ini_puti()
 * \param Section     the name of the section to write the value in
 * \param Key         the name of the entry to write
 * \param Value       the value to write
 * \param Filename    the name and full path of the .ini file to write to
 *
 * \return            1 if successful, otherwise 0
 */
SceBool ini_puti(const char *Section, const char *Key, SceInt32 Value, const char *Filename)
{
  char LocalBuffer[32];
  ini_atoi(LocalBuffer, sizeof(LocalBuffer), Value);
  return ini_puts(Section, Key, LocalBuffer, Filename);
}

/** ini_putf()
 * \param Section     the name of the section to write the value in
 * \param Key         the name of the entry to write
 * \param Value       the value to write
 * \param Filename    the name and full path of the .ini file to write to
 *
 * \return            1 if successful, otherwise 0
 */
SceBool ini_putf(const char *Section, const char *Key, SceFloat Value, const char *Filename)
{
  char LocalBuffer[64];
  ini_ftoa(LocalBuffer, sizeof(LocalBuffer), Value);
  return ini_puts(Section, Key, LocalBuffer, Filename);
}

/** ini_putbool()
 * \param Section     the name of the section to write the value in
 * \param Key         the name of the entry to write
 * \param Value       the value to write; it should be 0 or 1.
 * \param Filename    the name and full path of the .ini file to write to
 *
 * \return            1 if successful, otherwise 0
 */
SceBool ini_putbool(const char *Section, const char *Key, SceBool Value, const char *Filename)
{
  return ini_puts(Section, Key, Value ? "true" : "false", Filename);
}

#endif /* !INI_READONLY */