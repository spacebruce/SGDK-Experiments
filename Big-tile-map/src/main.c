#include <genesis.h>
#include <resources.h>

#include <mapHandler.h>

Map* bigmap;
int16_t cameraX, cameraY;
int16_t cameraVX, cameraVY;

uint16_t shakeMap()
{
    uint16_t maxTiles = 0;
    int16_t originalX = cameraX, originalY = cameraY;
    
    for (int16_t y = 0; y <= 920 - 224; y += 8)
    {
        for (int16_t x = 0; x <= 3840 - 320; x += 8)
        {
            MAP_scrollTo(bigmap, x, y);
            SYS_doVBlankProcess(); 
            
            uint16_t tilesUsed = tileCache_getUsage();
            if (tilesUsed > maxTiles)
                maxTiles = tilesUsed;

            tileCache_print();
            char text[32];
            sprintf(text, "max : %d", maxTiles);
            VDP_drawText(text, 0,1);
        }
    }
    
    // Restore original camera position
    MAP_scrollTo(bigmap, originalX, originalY);
    SYS_doVBlankProcess();
    
    return maxTiles;
}

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

    uint16_t vram = 1;

    bigmap = MAP_create(&mBigMap, BG_B, 0);
    vram = tileCache_init(vram, &tsBigMap, 64 * 5);
    MAP_setDataPatchCallback(bigmap, tileCache_callback);

    SPR_init();

    PAL_setColors(0, pBigMap.data, 48, DMA);

    //shakeMap();
    uint16_t debugFrames = 10;
    while(1)
    {      
        cameraX += cameraVX;
        cameraY += cameraVY;
        cameraX = clamp(cameraX, 0, 3840 - 320);
        cameraY = clamp(cameraY, 0, 920 - 224);

        MAP_scrollTo(bigmap, cameraX,cameraY);
        
        SPR_update();

        --debugFrames;
        if(debugFrames == 0)
        {
            tileCache_print();
            debugFrames = 10;
        }

        SYS_doVBlankProcess();        
    }
}