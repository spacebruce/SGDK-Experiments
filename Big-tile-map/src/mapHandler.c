#include "mapHandler.h"
#include <resources.h>
#include <kdebug.h>

#define SCREEN_TILES_W 42
#define SCREEN_TILES_H 32

// Globals
uint16_t bump = 0;
uint16_t lowestFree = 0;    
uint16_t highestFree = 0;
uint16_t ACTIVE_TILES;      // How many tiles to cache
uint16_t TOTAL_TILES;       // No. of tiles in map
#ifdef DEBUG
uint16_t MaxTilesEver;      // Max number ever needed for map
size_t memoryUsage;
#endif

uint16_t planeCache[SCREEN_TILES_W][SCREEN_TILES_H]; 
uint16_t* tileCache;     // Track plane slots in use
uint16_t* mapToPlane;   // mapTile -> planeTile
uint16_t* planeToMap;

uint16_t tileVram;          // vram offset
const TileSet* tileSet;     // tileset data

uint16_t tileCache_init(uint16_t vram, const TileSet* ts, uint16_t maxTiles)
{
    tileVram = vram;
    ACTIVE_TILES = maxTiles;
    tileSet = ts;

    if(ts->compression != COMPRESSION_NONE)
    {
        SYS_die("tile streamer requires NONE compression");
    }
    
    TOTAL_TILES = tileSet->numTile;
#ifdef DEBUG
    MaxTilesEver = 0;
#endif

    // Allocate memory
    const size_t tileCacheSize = ACTIVE_TILES * sizeof(uint16_t);
    const size_t lookup1Size = TOTAL_TILES * sizeof(uint16_t);
    const size_t lookup2Size = ACTIVE_TILES * sizeof(uint16_t);

    tileCache = (uint16_t*)malloc(tileCacheSize);
    mapToPlane = (uint16_t*)malloc(lookup1Size);
    planeToMap = (uint16_t*)malloc(lookup2Size);

    #ifdef DEBUG
    memoryUsage = tileCacheSize + lookup1Size + lookup2Size;
    kprintf("memory usage : %i", memoryUsage);
    #endif

    if (!tileCache || !mapToPlane || !planeToMap) 
    {
        kprintf("Memory allocation failed!");
        return 0xFFFF;
    }

    for (int i = 0; i < ACTIVE_TILES; i++)
    {
        tileCache[i] = 0;
        planeToMap[i] = 0xFFFF;
    }
    memset(planeCache, 0xFF, sizeof(planeCache));    
    memset(mapToPlane, 0xFF, TOTAL_TILES * sizeof(uint16_t));      

    bump = 0;
    lowestFree = 0;
    highestFree = ACTIVE_TILES - 1;
    
    return (vram + ACTIVE_TILES);
}

void tileCache_free()
{
    free(tileCache);
    free(mapToPlane);
    free(planeToMap);
}

uint16_t tileCache_fetchTile(uint16_t mapTile)
{
    if (mapTile >= TOTAL_TILES) return 0xFFFF;

    uint16_t planeTile = mapToPlane[mapTile];
    
    if (planeTile != 0xFFFF) // Already allocated
    {
        tileCache[planeTile]++;
        return planeTile;
    }

    if (bump < ACTIVE_TILES)
        planeTile = bump++;
    else 
    {
        planeTile = 0xFFFF;
        for (int i = lowestFree; i < ACTIVE_TILES; i++)
        {
            if (tileCache[i] == 0)
            {
                planeTile = i;
                lowestFree = i + 1;
                if (planeTile == highestFree)
                {
                    while (highestFree > 0 && tileCache[highestFree] > 0)
                        highestFree--;
                }
                break;
            }
        }
    }

    if (planeTile == 0xFFFF) 
    {
        kprintf("out of tile memory!!");
        return 0xFFFF;
    }

    // Init tile in caches
    mapToPlane[mapTile] = planeTile;
    planeToMap[planeTile] = mapTile;
    tileCache[planeTile] = 1;
    
    DMA_transfer(DMA, DMA_VRAM, &tileSet->tiles[mapTile * 8], (tileVram + planeTile) * 32, 16, 2);

    return planeTile;
}

void tileCache_releaseTile(uint16_t planeTile)
{
    if (planeTile == 0xFFFF) return; 

    if (tileCache[planeTile] > 0)
        tileCache[planeTile]--;

    if (tileCache[planeTile] == 0) 
    {
        // reverse lookup
        uint16_t mapTile = planeToMap[planeTile];
        if (mapTile != 0xFFFF)
        {
            mapToPlane[mapTile] = 0xFFFF;
            planeToMap[planeTile] = 0xFFFF;
        }

        if (planeTile < lowestFree)
            lowestFree = planeTile;
        if (planeTile > highestFree)
            highestFree = planeTile;
        if (planeTile + 1 == bump)
        {
            kprintf("bump!");
            bump--;
        }
    }
}


void tileCache_print()
{
#ifdef DEBUG
    uint16_t usedTiles = 0;
    for (int i = 0; i < ACTIVE_TILES; i++)
    {
        if (tileCache[i] > 0)
            usedTiles++;
    }

    if(usedTiles > MaxTilesEver)
        MaxTilesEver = usedTiles;

    char text[64];
    sprintf(text, "%04d/%04d (%04d)", usedTiles, ACTIVE_TILES, MaxTilesEver);
    VDP_drawText(text, 0,0);
#endif
}


void tileCache_callback(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size)
{
    uint16_t start = VDP_getAdjustedVCounter();
    #ifdef DEBUG
    #endif

    u16* dst = buf;
    u16 i = size;

    uint16_t xt = x;    // % SCREEN_TILES_W;
    while(xt >= SCREEN_TILES_W)
        xt -= SCREEN_TILES_W;
    uint16_t yt = y % SCREEN_TILES_H;

    while(i--)
    {
        u16 tileData = *dst;

        uint16_t tileIndex = tileData & TILE_INDEX_MASK;
        uint16_t oldTile = planeCache[xt][yt];

        //if(oldTile != tileIndex) // Buggy for some reason...
        {
            // If plane slot was previously occupied:
            if (oldTile != 0xFFFF)
            {
                tileCache_releaseTile(oldTile); // Counter
                planeCache[xt][yt] = 0xFFFF;    // Mark as not used so fetch doesn't use it
            }
            tileIndex = tileCache_fetchTile(tileIndex);
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
        {
            ++xt;
            if(xt >= SCREEN_TILES_W)
                xt = 0;
        }
        else
        {
            ++yt;
            if(yt >= SCREEN_TILES_H)
                yt = 0;
        }
        dst++;
    }

    uint16_t count = VDP_getAdjustedVCounter() - start;
    kprintf("lines : %i", count);
    #ifdef DEBUG
    //kprintf("Tiles DMA'd : %d", allocFrame);
    #endif
}

uint16_t tileCache_getUsage()
{
    #ifdef DEBUG
    return memoryUsage;
    #endif
    return 0x0000;
}