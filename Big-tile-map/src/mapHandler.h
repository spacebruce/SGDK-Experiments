#pragma once

#include <genesis.h>

uint16_t tileCache_init(uint16_t vram, const TileSet* ts, uint16_t maxTiles);
void tileCache_free();
void tileCache_callback(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size);

//Debug
void printTileCacheUsage();
uint16_t getTilesMaxUsage();