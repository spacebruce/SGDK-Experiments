 #include <genesis.h>
#include <resources.h>

int main(bool resetType) 
{
    if (!resetType)
        SYS_hardReset();

    VDP_setScreenHeight224();
    VDP_setScreenWidth320();

    SPR_init();

    while(1)
    {      
        SPR_update();
        SYS_doVBlankProcess();        
    }
}
