/*  Glue functions for the minIni library, based on the C/C++ stdio library
 *
 *  Or better said: this file contains macros that maps the function interface
 *  used by minIni to the standard C/C++ file I/O functions.
 *  Here they are replaced by the PSPSDK file I/O functions.
 *
 *  By CompuPhase, 2008-2014
 *  PSP "port" by danssmnt, 2025
 *  This "glue file" is in the public domain. It is distributed without
 *  warranties or conditions of any kind, either express or implied.
 */

/* map required PSPSDK file I/O types */
#include <psptypes.h>
#include <pspiofilemgr.h>

#include <stdio.h>
#include <stdlib.h>

#define INI_FILETYPE                    SceUID

/* Mimic fgets behavior with PSPSDK functions
 * Returns the equivalent to fgets
 * Thx go to Freakler for his code: https://github.com/Freakler/CheatDeviceRemastered/blob/d537e30f6fb927cc873e5756c7a4afe07c267c93/source/minIni.c#L96
 */
extern SceBool psp_read_fgets(char *s, SceSize n, INI_FILETYPE *stream);

#define ini_openread(filename,file)     ((*(file) = sceIoOpen((filename), PSP_O_RDONLY, 0777)) >= 0)
#define ini_openwrite(filename,file)    ((*(file) = sceIoOpen((filename), PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777)) >= 0)
#define ini_openrewrite(filename,file)  ((*(file) = sceIoOpen((filename), PSP_O_RDWR, 0777)) >= 0)
#define ini_close(file)                 (sceIoClose(*(file)) >= 0)
#define ini_read(buffer,size,file)      psp_read_fgets((buffer), (size), (file))
#define ini_write(buffer,size,file)     (sceIoWrite(*(file), (buffer), (size)) >= 0)
#define ini_rename(source,dest)         (sceIoRename((source), (dest)) >= 0)
#define ini_remove(filename)            (sceIoRemove((filename)) >= 0)

#define INI_FILEPOS                     SceInt32
#define ini_tell(file,pos)              ((*(pos) = sceIoLseek32(*(file), 0, PSP_SEEK_CUR)) >= 0)
#define ini_seek(file,pos)              ((*(pos) = sceIoLseek32(*(file), *(pos), PSP_SEEK_SET)) >= 0)

#define ini_atoi(string,size,value)     snprintf((string), (size), "%d", (value))
#define ini_ftoa(string,size,value)     snprintf((string), (size), "%f", (value))
#define ini_atof(string)                strtof((string), NULL)