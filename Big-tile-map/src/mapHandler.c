#include "mapHandler.h"
#include <resources.h>
#include <kdebug.h>

#define SCREEN_TILES_W 42
#define SCREEN_TILES_H 32

typedef struct
{ 
    uint16_t mapTile; // actual tilemap id
    uint16_t planeTile; // virtual id on plane
    uint16_t count; // refcount
} TileMatch_t;

// utter mess
uint16_t bump = 0;
uint16_t lowestFree = 0;    
uint16_t highestFree = 0;
uint16_t ACTIVE_TILES;      // How many tiles to cache
uint16_t TOTAL_TILES;       // No. of tiles in map
uint16_t MaxTilesEver;      // Max number ever needed for map (debug)
TileMatch_t* tileCache;     // Track mapping between plane n map
uint16_t planeCache[SCREEN_TILES_W][SCREEN_TILES_H];    // Do it in plane space to make revokes faster
uint16_t* mapTileToPlaneTile;   // What plane tile is used to represent this map tile? (MEMORY HOG!!!)
uint16_t tileVram;          // vram offset
const TileSet* tileSet;     // tileset data

uint16_t tileCache_init(uint16_t vram, const TileSet* ts, uint16_t maxTiles)
{
    tileVram = vram;
    ACTIVE_TILES = maxTiles;
    tileSet = ts;
    
    TOTAL_TILES = tileSet->numTile;

    MaxTilesEver = 0;

    kprintf("Alloc %d + %d : %d bytes", ACTIVE_TILES, TOTAL_TILES, (ACTIVE_TILES + TOTAL_TILES));
    
    // Allocate handler memory
    uint16_t MaxTileMemory = ACTIVE_TILES * sizeof(TileMatch_t);
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
    for (int16_t i = 0; i < ACTIVE_TILES; i++)
    {
        tileCache[i].mapTile = 0xFFFF;
        tileCache[i].planeTile = 0xFFFF;
        tileCache[i].count = 0;
    }
    // Clear out buffers
    memset(planeCache, 0xFF, (SCREEN_TILES_H * SCREEN_TILES_W) * 2);    
    memset(mapTileToPlaneTile, 0xFF, TOTAL_TILES * sizeof(uint16_t));      
    
    lowestFree = 0;
    highestFree = ACTIVE_TILES - 1;
    
    return (vram + ACTIVE_TILES);
}

void tileCache_free()
{
    free(tileCache);
    free(mapTileToPlaneTile);
}

static int allocFrame = 0;
uint16_t tileCache_fetchTile(uint16_t mapTile)
{
    if (mapTile >= TOTAL_TILES) return 0xFFFF;

    uint16_t planeTile = mapTileToPlaneTile[mapTile];
    
    if (planeTile != 0xFFFF) // Already allocated
    {
        tileCache[planeTile].count++;
        return planeTile;
    }

    // Starting out, use bump allocator
    if (bump < ACTIVE_TILES)
    {
        planeTile = bump++;
    }
    else 
    {
        for (int i = lowestFree; i < ACTIVE_TILES; i++)
        {
            if (tileCache[i].count == 0)
            {
                planeTile = i;
                lowestFree = i + 1;
                if (planeTile == highestFree)
                {
                    while (highestFree > 0 && tileCache[highestFree].count > 0)
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

    // Init tile in cache
    mapTileToPlaneTile[mapTile] = planeTile;        // Here
    tileCache[planeTile].mapTile = mapTile;         // Here
    tileCache[planeTile].planeTile = planeTile;     // and Here
    tileCache[planeTile].count = 1;
    
    DMA_transfer(DMA, DMA_VRAM, &tileSet->tiles[mapTile * 8], (tileVram + planeTile) * 32, 16, 2);
    
    ++allocFrame;

    return planeTile;
}

void tileCache_releaseTile(uint16_t planeTile)
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
        
        // Help speed up next search
        if (planeTile < lowestFree)
        {
            lowestFree = planeTile;
        }
        
        if (planeTile > highestFree)
        {
            highestFree = planeTile;
        }

        if (planeTile + 1 == bump)
        {
            bump--;
        }

        // test : check integrity by blanking out unused tiles
        //uint16_t blankTile[16] = {0};
        //DMA_transfer(DMA, DMA_VRAM, blankTile, (tileVram + planeTile) * 32, 16, 2);
    }
}

void tileCache_print()
{
    uint16_t usedTiles = 0;
    for (int i = 0; i < ACTIVE_TILES; i++)
    {
        if (tileCache[i].count > 0)
        {
            usedTiles++;
        }
    }

    if(usedTiles > MaxTilesEver)
    {
        MaxTilesEver = usedTiles;
    }

    char text[64];
    sprintf(text, "%04d/%04d (%04d)", usedTiles, ACTIVE_TILES, MaxTilesEver);
    VDP_drawText(text, 0,0);
    //sprintf(text, "Max : %d", MaxTilesEver);
    //VDP_drawText(text, 0,1);
}

void tileCache_callback(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size)
{
    //allocFrame = 0;
    //KDebug_StartTimer();
    u16* dst = buf;
    u16 i = size;

    uint16_t xt = x % SCREEN_TILES_W;
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
                tileCache_releaseTile(oldTile); // Deplete its counter
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
    //KDebug_StopTimer();
    //kprintf("Tiles DMA'd : %d", allocFrame);
}

uint16_t tileCache_getUsage()
{
    return MaxTilesEver;
}