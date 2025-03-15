#include <genesis.h>
#include <resources.h>

#include <mapHandler.h>

Map* bigmap;
int16_t cameraX, cameraY;
int16_t cameraVX, cameraVY;

int main(bool resetType) 
{
    if (!resetType)
        SYS_hardReset();

    VDP_setScreenHeight224();
    VDP_setScreenWidth320();

    SYS_showFrameLoad(true);

    void gameJoystickHandler(u16 joy, u16 changed, u16 state)
    {
        if(joy == JOY_1)
        {
            if(changed & BUTTON_LEFT) cameraVX += (-1 * ((state & BUTTON_LEFT) != 0));
            if(changed & BUTTON_RIGHT) cameraVX += (1 * ((state & BUTTON_RIGHT) != 0));
            if(changed & BUTTON_UP) cameraVY += (-1 * ((state & BUTTON_UP) != 0));
            if(changed & BUTTON_DOWN) cameraVY += (1 * ((state & BUTTON_DOWN) != 0));  
        }
    };
    JOY_setEventHandler(gameJoystickHandler);

    bigmap = MAP_create(&mBigMap, BG_B, 0);

    uint16_t vram = 1;
    vram = initTileCache(vram);
    MAP_setDataPatchCallback(bigmap, MapCB);

    SPR_init();

    PAL_setColors(0, pBigMap.data, 16, DMA);

    MAP_setDataPatchCallback(bigmap, MapCB);

    uint16_t cameraTX, cameraTY;
    while(1)
    {      
        cameraX += cameraVX;
        cameraY += cameraVY;
        cameraX = clamp(cameraX, 0, 1024 - 320);
        cameraY = clamp(cameraY, 0, 1024 - 224);

        MAP_scrollTo(bigmap, cameraX,cameraY);
        
        uint16_t cameraTTX = cameraX / 8;
        uint16_t cameraTTY = cameraY / 8;
        if(cameraTX != cameraTTX || cameraTTY != cameraTY)
        {
            cameraTX = cameraTTX;
            cameraTY = cameraTTY;
        }

        SPR_update();
        SYS_doVBlankProcess();        
    }
}