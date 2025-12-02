# pspIni
This is a fork of [minIni](https://github.com/compuphase/minIni) by _[@compuphase](https://github.com/compuphase)_ developed more specifically for the _PlayStation: Portable_.

It doesn't change much of the original, just removes some unnecessary features and retypes / converts functions.

This fork is intended for anyone who wants a light, quick and easy way to parse ``.ini`` files with PSPSDK without having to manually write the PSP ``minGlue.h`` and all that boring stuff.

Should work with all PSPSDKs, with ``USE_PSPSDK_LIBC = 1`` and all warning flags enabled (even ``-Wpedantic``!).

## Limitations
 - Doesn't support wide chars (not planned for implementation)


## Extremely Basic Sample Code

``file.ini``:
```
[PlayerInfo]
Name = PSP Guy
Country = Antartican
Favorite Number = 93
```


``main.c``:
```
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>

#include "callbacks.h"
#include "pspIni/minIni.h"

PSP_MODULE_INFO("Reading INIs", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define TRUE 1

int main(void)
{
    // In a callbacks.h file
    setup_callbacks();

    // INI file path
    const char* file_path = "file.ini";

    // String Buffers
    char player_name[32];
    char player_country[32];
    
    // Get Info from INI file
    ini_gets("PlayerInfo", "Name", "User", player_name, sizeof(player_name), file_path);
    ini_gets("PlayerInfo", "Country", "International Waters", player_country, sizeof(player_country), file_path);
    int player_fav_num = ini_geti("PlayerInfo", "Favorite Number", -1, file_path);

    pspDebugScreenInit();

    while (TRUE)
    {
        pspDebugScreenSetXY(0, 0);

        // Draw values read from INI
        pspDebugScreenPrintf("Read from INI:\n\n");
        pspDebugScreenPrintf("[PlayerInfo]\n\n");
        pspDebugScreenPrintf("Name = %s\n", player_name);
        pspDebugScreenPrintf("Country = %s\n", player_country);
        pspDebugScreenPrintf("Favorite Number = %i\n", player_fav_num);

        sceDisplayWaitVblankStart();
    }

    return 0;
}
```

Output:
<img src="pictures/SampleOutput.png">

---

## Acknowledgments
 - _Thiadmer Riemersma ([@compuphase](https://github.com/compuphase)), Steven Van Ingelgem, Luca
Bassanello_ - for creating and maintaining minIni!
 - _[Freakler](https://github.com/Freakler)_ - writing a part of ``psp_fgets`` for CheatDevice: Remastered.