#include "mapHandler.h"
#include <resources.h>

#define SCREEN_TILES_W 42
#define SCREEN_TILES_H 32

typedef struct
{ 
    uint16_t mapTile; 
    uint16_t planeTile; 
    uint16_t count; // refcount
} TileMatch_t;

uint16_t bump = 0;
uint16_t lowestFree = 0;
uint16_t highestFree = 0;
uint16_t MAX_TILES;
uint16_t TOTAL_TILES;
uint16_t MaxTilesEver;

TileMatch_t* tileCache; 
uint16_t planeCache[SCREEN_TILES_W][SCREEN_TILES_H];
uint16_t* mapTileToPlaneTile; // Lookup table for fast access
uint16_t tileVram;
TileSet* tileSet;
Map* tileMap;

uint16_t tileCache_init(uint16_t vram, TileSet* ts, uint16_t maxTiles)
{
    tileVram = vram;
    MAX_TILES = maxTiles;
    tileSet = ts;
    
    TOTAL_TILES = tileSet->numTile;

    MaxTilesEver = 0;
    
    // Allocate handler memory
    uint16_t MaxTileMemory = MAX_TILES * sizeof(TileMatch_t);
    uint16_t TotalTileMemory = TOTAL_TILES * sizeof(uint16_t);
    tileCache = (TileMatch_t*)malloc(MaxTileMemory);
    mapTileToPlaneTile = (uint16_t*)malloc(TotalTileMemory);
    
    if (!tileCache || !mapTileToPlaneTile) 
    {
        kprintf("Memory allocation failed!");
        return 0xFFFF;
    }

    kprintf("Memory used : %d", TotalTileMemory + MaxTileMemory);

    // Invalidate everything
    for (int16_t i = 0; i < MAX_TILES; i++)
    {
        tileCache[i].mapTile = 0xFFFF;
        tileCache[i].planeTile = 0xFFFF;
        tileCache[i].count = 0;
    }
    memset(planeCache, 0xFF, (SCREEN_TILES_H * SCREEN_TILES_W) * 2);    
    // allocator
    memset(mapTileToPlaneTile, 0xFF, TOTAL_TILES * sizeof(uint16_t));      
    
    return (vram + MAX_TILES);
}

uint16_t fetchTile(uint16_t mapTile)
{
    if (mapTile >= TOTAL_TILES) return 0xFFFF;

    uint16_t planeTile = mapTileToPlaneTile[mapTile];
    
    if (planeTile != 0xFFFF) // Already allocated
    {
        tileCache[planeTile].count++;
        return planeTile;
    }

    // Use bump allocator when starting out
    if (bump < MAX_TILES)
    {
        planeTile = bump++;
    }
    else // Find free slot
    {
        for (int i = lowestFree; i < MAX_TILES; i++)
        {
            if (tileCache[i].count == 0)
            {
                planeTile = i;
                lowestFree = i + 1;
                break;
            }
        }
    }

    // 
    if (planeTile == 0xFFFF) 
    {
        kprintf("out of tile memory!!");
        return 0xFFFF;
    }

    // Transfer tile to VRAM
    mapTileToPlaneTile[mapTile] = planeTile;
    tileCache[planeTile].mapTile = mapTile;
    tileCache[planeTile].planeTile = planeTile;
    tileCache[planeTile].count = 1;
    
    DMA_transfer(DMA, DMA_VRAM, &tileSet->tiles[mapTile * 8], (tileVram + planeTile) * 32, 16, 2);
    
    return planeTile;
}

void releaseTile(uint16_t planeTile)
{
    if (planeTile == 0xFFFF) return; 

    // --ref
    if (tileCache[planeTile].count > 0)
    {
        tileCache[planeTile].count--;
    }

    // Fully deallocated
    if (tileCache[planeTile].count == 0) 
    {
        // Remove from caches and lookups
        mapTileToPlaneTile[tileCache[planeTile].mapTile] = 0xFFFF; 
        tileCache[planeTile].mapTile = 0xFFFF;
        tileCache[planeTile].planeTile = 0xFFFF;
        
        // Help speedup next search
        if (planeTile < lowestFree)
        {
            lowestFree = planeTile;
        }
    }
}

void tileCache_free()
{
    free(tileCache);
    free(mapTileToPlaneTile);
}


void printTileCacheUsage()
{
    uint16_t usedTiles = 0;
    for (int i = 0; i < MAX_TILES; i++)
    {
        if (tileCache[i].count > 0)
        {
            usedTiles++;
        }
    }

    if(usedTiles > MaxTilesEver)
    {
        MaxTilesEver = usedTiles;
        kprintf("new max : %d", MaxTilesEver);
    }

    char text[64];
    sprintf(text, "%04d/%04d", usedTiles, MAX_TILES);
    VDP_drawText(text, 0,0);
    //sprintf(text, "Max : %d", MaxTilesEver);
    //VDP_drawText(text, 0,1);
}

void tileCache_callback(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size)
{
    u16* dst = buf;
    u16 i = size;

    while(i--)
    {
        u16 tileData = *dst;
        uint16_t tileIndex = tileData & TILE_INDEX_MASK;

        uint16_t xt = x % SCREEN_TILES_W;
        uint16_t yt = y % SCREEN_TILES_H;

        uint16_t oldTile = planeCache[xt][yt];

        //if(oldTile != tileIndex)  // Causes bugs 
        {
            // If plane slot was previously occupied:
            if (oldTile != 0xFFFF)
            {
                releaseTile(oldTile);           // Deplete its counter
                planeCache[xt][yt] = 0xFFFF;    // Mark as not used so fetch doesn't use it
            }
            tileIndex = fetchTile(tileIndex);
            planeCache[xt][yt] = tileIndex;
        }

        if (tileIndex != 0xFFFF) 
        {
            *dst = (tileData & ~TILE_INDEX_MASK) | (tileVram + tileIndex);
        }
        else
        {
            *dst = 0x0000;
        }

        if (updateType == ROW_UPDATE) 
            x++;
        else
            y++;
        dst++;
    }
}

uint16_t getTilesMaxUsage()
{
    return MaxTilesEver;
}