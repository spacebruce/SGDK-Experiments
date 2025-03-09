#include "mapHandler.h"
#include <resources.h>

#define MAX_TILES 512
#define TOTAL_TILES 2048  
#define SCREEN_TILES_W 64 // 
#define SCREEN_TILES_H 32 // 

uint16_t bump = 0;
uint16_t alloc = 0;
uint16_t tileCache[TOTAL_TILES]; 
uint16_t tileCacheInverted[TOTAL_TILES]; 
uint16_t screenCache[SCREEN_TILES_W][SCREEN_TILES_H];

void initTileCache()
{
    for (int i = 0; i < TOTAL_TILES; i++)
        tileCache[i] = 0xFFFF; 
}

// slow and shit?
bool isTileInUse(uint16_t vramSlot)
{
    for (int x = 0; x < SCREEN_TILES_W; x++)
    {
        for (int y = 0; y < SCREEN_TILES_H; y++)
        {
            if (screenCache[x][y] == vramSlot)
                return true; // Tile is still in use
        }
    }
    return false; // Tile can be reused
}

uint16_t loadTile(uint16_t index, uint16_t screenX, uint16_t screenY)
{
    // Already in VRAM
    if (tileCache[index] != 0xFFFF)
        return tileCache[index];

    uint16_t vramSlot;

    if (bump < MAX_TILES)
    {
        vramSlot = bump;
        ++bump;
    }
    else
    {
        // Try to reuse the tile currently at this screen position
        vramSlot = screenCache[screenX % SCREEN_TILES_W][screenY % SCREEN_TILES_H];

        // If it's still in use elsewhere, find a different reusable tile
        while (isTileInUse(vramSlot))
        {
            vramSlot = (vramSlot + 1) % MAX_TILES;
        }
        //
        uint16_t oldIndex = tileCacheInverted[vramSlot];
        tileCache[oldIndex] = 0xFFFF; 
        tileCacheInverted[vramSlot] = 0xFFFF;
    }

    tileCache[index] = vramSlot;
    tileCacheInverted[vramSlot] = index;

    screenCache[screenX % SCREEN_TILES_W][screenY % SCREEN_TILES_H] = vramSlot;

    DMA_transfer(DMA, DMA_VRAM, &tsBigMap.tiles[index * 8], vramSlot * 32, 16, 2);
    return vramSlot;
}


void MapCB(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size)
{
    u16* dst = buf;
    u16 xt = x;
    u16 yt = y;
    uint16_t xtt, ytt;
    u16 i = size;

    while(i--)
    {
        u16 tileData = *dst;
        uint16_t tileIndex = tileData & TILE_INDEX_MASK;

        uint16_t vramSlot = loadTile(tileIndex, xt, yt);

        if (updateType == ROW_UPDATE) 
        {
            xt++;
        }
        else
        {
            yt++;
        }

        *dst++ = (tileData & ~TILE_INDEX_MASK) | vramSlot;

    }
}