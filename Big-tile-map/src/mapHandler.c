#include "mapHandler.h"
#include <resources.h>

#define MAX_TILES 256   // VRAM capacity
#define TOTAL_TILES 2048  
#define SCREEN_TILES_W 64 // 
#define SCREEN_TILES_H 32 // 

uint16_t bump = 0;
uint16_t tileCache[TOTAL_TILES]; 

void initTileCache()
{
    for (int i = 0; i < TOTAL_TILES; i++)
        tileCache[i] = 0xFFFF; 
}

uint16_t fetchTile(uint16_t index)
{
     // Already in VRAM
    if (tileCache[index] != 0xFFFF)
        return tileCache[index]; 

    // Out of space
    if (bump >= MAX_TILES)
        return 0; 

    uint16_t vramSlot = bump++;
    tileCache[index] = vramSlot;
    DMA_transfer(CPU, DMA_VRAM, &tsBigMap.tiles[index * 8], vramSlot * 32, 16, 2);
    return vramSlot;
}

void MapCB(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size)
{
    u16* dst = buf;
    u16 xt = x;
    u16 yt = y;
    u16 i = size;

    while(i--)
    {
        u16 tileData = *dst;
        uint16_t tileIndex = tileData & TILE_INDEX_MASK;

        uint16_t vramSlot = fetchTile(tileIndex);

        *dst++ = (tileData & ~TILE_INDEX_MASK) | vramSlot;

        if (updateType == ROW_UPDATE) 
            xt++;
        else
            yt++;
    }
}