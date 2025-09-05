
#include <types.h>
#include <tools.h>
#include <memory.h>
#ifdef TC_DEBUG
#include <kdebug.h>
#endif
#include "tileCache.h"

#define SCREEN_TILES_W 64
#define SCREEN_TILES_H 32

#define TC_ASSUME(x)\
    if(!(x)) {\
        __builtin_unreachable();\
    }

uint16_t ACTIVE_TILES;        // cache size (# plane slots we manage)
uint16_t TOTAL_TILES;         // total tiles in the tileset

uint16_t tileVramBase;        // plane tile index base (in 8x8 tiles)
const TileSet* tileSource;    // source tileset (no compression)

// Cache state
uint16_t reservedTiles; 
uint16_t* memBlock;     // POINT THIS AT SOME target USEFUL OR MALLOC IT
uint16_t* refcnt;       // how many copies of a tile active
uint16_t* mapToVRAM;    // base map -> offset
uint16_t* VRAMToMap;    // offset -> map
uint16_t* freeStack;    // Stack of remaining tile slots 
uint16_t  freeTop;      //
bool mallocMode;        

uint16_t planeCache[SCREEN_TILES_W * SCREEN_TILES_H]; // stores plane slot indices (or 0xFFFF)

size_t memoryUsage;

#ifdef TC_DEBUG
uint16_t MaxTilesEver;
uint16_t TilesInUse;
#endif

inline void push_free(uint16_t slot)
{
    TC_ASSUME(slot < 2048);
    if (slot >= reservedTiles)
    {
        VRAMToMap[slot] = 0xFFFF; 
        freeStack[freeTop++] = slot;
    }
}
inline uint16_t pop_free(void)
{
    if (freeTop == 0) 
        return 0xFFFF; 
    return freeStack[--freeTop];
}

void tileCache_free()
{
    if(mallocMode)
        MEM_free(memBlock);
}

inline uint16_t tileCache_claim(uint16_t tileIndex)
{
    TC_ASSUME(tileIndex < 2048);

    // if reserved id, skip
    if (tileIndex < reservedTiles)
    {
        return tileIndex;
    }

    // already in use
    uint16_t planeSlot = mapToVRAM[tileIndex];
    if (planeSlot != 0xFFFF)
    {
        ++refcnt[planeSlot];
        return planeSlot;
    }

    // grab new tile
    planeSlot = pop_free();
    if (planeSlot == 0xFFFF)
    {
        #ifdef TC_DEBUG
            kprintf("can't allocate tile, make the buffer bigger!");
        #endif
        return 0x0000;
    }

    // map it both ways
    mapToVRAM[tileIndex] = planeSlot;
    VRAMToMap[planeSlot] = tileIndex;
    refcnt[planeSlot] = 1;

    // load
    const uint16_t target = (tileVramBase + planeSlot);

    //if(DMA_canQueue(DMA_VRAM, 16))
    VDP_loadTileData(&tileSource->tiles[tileIndex * 8], target, 1, CPU);
    //else
    //    VDP_loadTileData(&tileSource->tiles[tileIndex * 8], target, 1, CPU);

    return planeSlot;
}

inline void tileCache_releaseTile(uint16_t planeSlot)
{
    TC_ASSUME(planeSlot < 2048);

    // reserved tiles are permanent
    if (planeSlot < reservedTiles)
        return; 

    // reduce tile counter
    uint16_t c = refcnt[planeSlot] - 1; 
    refcnt[planeSlot] = c;

    // return vram slot to pool if expired
    if (c == 0)
    {
        const uint16_t tileIndex = VRAMToMap[planeSlot];
        if (tileIndex != 0xFFFF)
            mapToVRAM[tileIndex] = 0xFFFF;
        push_free(planeSlot);
    }
}


uint16_t tileCache_getUsage(void)
{
#ifdef TC_DEBUG
    return (uint16_t)memoryUsage;
#else
    return 0;
#endif
}

void tileCache_print(void)
{
#ifdef TC_DEBUG
    uint16_t used = 0;
    for (uint16_t i = 0; i < ACTIVE_TILES; i++)
    {
        if (refcnt[i] > 0)
            ++used;
    }

    if (used > MaxTilesEver)
        MaxTilesEver = used;

    kprintf("Total slots    : %u", ACTIVE_TILES);
    kprintf("Reserved slots : %u", reservedTiles);
    kprintf("Dynamic slots  : %u", ACTIVE_TILES - reservedTiles);
    kprintf("Dynamic in use : %u", used);
    kprintf("Dynamic free   : %u", freeTop);
    kprintf("Max ever used  : %u", MaxTilesEver);
    kprintf("VRAM base tile : %u", tileVramBase);
    kprintf("Tileset total  : %u", TOTAL_TILES);
    kprintf("Memory usage   : %lu bytes", (u32)memoryUsage);
#endif
}

void tileCache_callback(Map *map, uint16_t *buf, uint16_t x, uint16_t y, MapUpdateType updateType, uint16_t size)
{
    //TC_ASSUME(size == 28 || size == 30 || size == 40);
    TC_ASSUME(updateType == ROW_UPDATE || updateType == COLUMN_UPDATE);

    uint16_t i = size;
    uint16_t* dst = buf;

    uint16_t xt = x;    // % SCREEN_TILES_W;
    while(xt >= SCREEN_TILES_W)
        xt -= SCREEN_TILES_W;
    uint16_t yt = y % SCREEN_TILES_H;

    #ifdef TC_DEBUG
    KDebug_StartTimer();
    #endif

    while (i--)
    {
        const uint16_t tileData  = *dst;
        const uint16_t tileIndex = (tileData & TILE_INDEX_MASK) - tileVramBase;
        const uint16_t cacheIndex = ((xt << 5) + yt);

        TC_ASSUME(tileIndex < 2048);

        const uint16_t oldTile = planeCache[cacheIndex];

        // tile already in plane cache in right spot, re-use it
        if (oldTile != 0xFFFF && VRAMToMap[oldTile] == tileIndex)
        {
            *dst = (tileData & ~TILE_INDEX_MASK) | (tileVramBase + oldTile);
        }
        else    
        {
            // release old tile 
            if (oldTile != 0xFFFF)
            {
                tileCache_releaseTile(oldTile);
                planeCache[cacheIndex] = 0xFFFF;
            }

            // grab new one
            const uint16_t newData = tileCache_claim(tileIndex); 
            planeCache[cacheIndex] = newData;

            *dst = 
                (tileData & ~TILE_INDEX_MASK)   // Merge map data tile properties
                | (tileVramBase + newData);     // with new tile address 
        }

        // next
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
        ++dst;
    }
    #ifdef TC_DEBUG
    KDebug_StopTimer();
    #endif
}

uint16_t tileCache_init(const Map* map,const TileSet* ts, uint16_t maxTiles, uint16_t reserve, uint8_t* where)
{
    tileVramBase   = map->baseTile & TILE_INDEX_MASK;
    ACTIVE_TILES   = maxTiles;
    TOTAL_TILES = ts->numTile;
    reservedTiles  = reserve;
    tileSource     = ts;
    #ifdef TC_DEBUG
        MaxTilesEver = 0;
    #endif

    if (!ts || ts->compression != COMPRESSION_NONE)
        SYS_die("tile streamer requires NONE compression");

    // Switch off if overallocated
    if (maxTiles >= TOTAL_TILES)
    {
        VDP_loadTileSet(ts, tileVramBase, DMA_QUEUE);
        return tileVramBase + maxTiles;
    }

    // Allocate
    size_t x = 0;
    const size_t refsz   = ACTIVE_TILES * sizeof(uint16_t);
    const size_t map1sz  = TOTAL_TILES  * sizeof(uint16_t);
    const size_t map2sz  = ACTIVE_TILES * sizeof(uint16_t);
    const size_t freesz  = ACTIVE_TILES * sizeof(uint16_t);
    memoryUsage = refsz + map1sz + map2sz + freesz;
    if(where == NULL)
    {
        kprintf("malloc mode");
        mallocMode = true;
        memBlock = (uint16_t*)MEM_alloc(memoryUsage);
    }
    else
    {
        kprintf("blob mode");
        mallocMode = false;
        memBlock = (uint16_t*)where;
    }
    refcnt    = &memBlock[x]; x += ACTIVE_TILES;
    mapToVRAM = &memBlock[x]; x += TOTAL_TILES;
    VRAMToMap = &memBlock[x]; x += ACTIVE_TILES;
    freeStack = &memBlock[x]; x += ACTIVE_TILES;

    #ifdef TC_DEBUG
        kprintf("tilecache mem: %u bytes", (u32)memoryUsage);
    #endif

    if (!refcnt || !mapToVRAM || !VRAMToMap || !freeStack)
    {
        SYS_die("tilecache alloc failed");
        return 0xFFFF;
    }

    // Init buffers
    memsetU16(VRAMToMap, 0xFFFF, ACTIVE_TILES);
    memsetU16(refcnt, 0x0000, ACTIVE_TILES);
    memsetU16(mapToVRAM, 0xFFFF, TOTAL_TILES);
    memsetU16(planeCache, 0xFFFF, (SCREEN_TILES_H * SCREEN_TILES_W));

    // load in reserved tiles
    if(reservedTiles > 0)
    {
        for (uint16_t i = 0; i < reservedTiles && i < TOTAL_TILES; i++)
        {
            mapToVRAM[i]   = i;     // map tile i -> fixed slot i
            VRAMToMap[i]   = i;     // no matter target you go... there you are
            refcnt[i]       = 1;    // jam refcnt to 1
        }
        VDP_loadTileData(&tileSource->tiles[0], tileVramBase, reservedTiles, DMA_QUEUE);
    }

    // Fill stack
    freeTop = 0;
    for (uint16_t i = ACTIVE_TILES; i-- > reservedTiles; )
    {
        freeStack[freeTop++] = i;
    }

    // Attach callback
    MAP_setDataPatchCallback(map, tileCache_callback);

    // Return first free VRAM tile index AFTER our managed block
    return (uint16_t)(tileVramBase + ACTIVE_TILES);
}

#undef TC_ASSUME
#undef SCREEN_TILES_W
#undef SCREEN_TILES_H