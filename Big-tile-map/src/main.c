#include <genesis.h>
#include <resources.h>

#include <tileCache.h>

// Demo
Map* bigmap;
int16_t cameraX, cameraY;
int16_t cameraVX, cameraVY;
int16_t mapWidth, mapHeight;

/// RIPPED FROM SGDK tile animation demo
#define TILE_FRAMES_NUM             2
#define TILE_FRAME_DELAY            10
s16 tileFrameIndex; // define current frame index for tiles
s16 frameTicks;     // define auxiliary frame counter
const TileSet *tile_anim[] =
{
    &animated_tileset_frame_0,
    &animated_tileset_frame_1
};
void TilesAnimationUpdate()
{
    if (frameTicks-- == 0)
    {
        // animate
        frameTicks = TILE_FRAME_DELAY;
        tileFrameIndex++;
        if (tileFrameIndex == TILE_FRAMES_NUM)
            tileFrameIndex = 0;

        // load frame
        VDP_loadTileSet(tile_anim[tileFrameIndex], bigmap->baseTile, DMA);
    }
}
/// ACHTUNG

int main(bool resetType) 
{
    if (!resetType)
        SYS_hardReset();

    // Keep screen off while loading
    VDP_setEnable(false);

    // Setup & demo junk
    VDP_setScreenHeight224();
    VDP_setScreenWidth320();
    SYS_showFrameLoad(true);
    void gameJoystickHandler(uint16_t joy, uint16_t changed, uint16_t state)
    {
        if(joy == JOY_1)
        {
            if(changed & BUTTON_LEFT) cameraVX += (-1 * ((state & BUTTON_LEFT) != 0));
            if(changed & BUTTON_RIGHT) cameraVX += (1 * ((state & BUTTON_RIGHT) != 0));
            if(changed & BUTTON_UP) cameraVY += (-1 * ((state & BUTTON_UP) != 0));
            if(changed & BUTTON_DOWN) cameraVY += (1 * ((state & BUTTON_DOWN) != 0));  

            if((changed & BUTTON_START) && (state & BUTTON_START))
                tileCache_print();
        }
    };
    JOY_setEventHandler(gameJoystickHandler);
    cameraX = 0;
    cameraY = 200;

    // Map loading
    const uint8_t palette = PAL1;
    uint16_t vram = 1;  // 
    bigmap = MAP_create(&mBigMap, BG_B, TILE_ATTR_FULL(palette, true, false, false, vram)); // <- load in as normal
    PAL_setColors(palette * 16, pBigMap.data, pBigMap.length, DMA);                         // Demo on PAL1 to demonstrate TILE_ATTR works properly
    VDP_setBackgroundColor((palette * 16) + 0);

    vram = tileCache_init(
        bigmap,         // Pointer to Map
        &tsBigMap,      // Pointer to tileset
        302,            // How many tiles to allocate
        animated_tileset_frame_0.numTile    // How many of those are static
    );
    
    // Bonus - Init sprites like this once you've figured out the background sizes to maximise your sprite memory 
    SPR_initEx(TILE_FONT_INDEX - vram);

    // Activate video
    VDP_setEnable(true);

    while(1)
    {      
        // Move around
        cameraX += cameraVX;
        cameraY += cameraVY;
        cameraX = clamp(cameraX, 0, (mBigMap.w * 128) - 320);
        cameraY = clamp(cameraY, 0, (mBigMap.h * 128) - 224);
        MAP_scrollTo(bigmap, cameraX,cameraY);

        // Demo stuff
        TilesAnimationUpdate();

        // Important 
        SPR_update();  
        SYS_doVBlankProcess();     
    }

    tileCache_free();
    MAP_release(bigmap);
}