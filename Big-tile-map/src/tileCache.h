#pragma once

#include <map.h>
#include <vdp_tile.h>

#define TC_DEBUG 1

uint16_t tileCache_init(const Map* map, const TileSet* ts, uint16_t maxTiles, uint16_t reserve);
void tileCache_free();
void tileCache_service();

void tileCache_print();
uint16_t tileCache_getUsage();