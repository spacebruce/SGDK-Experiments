#pragma once

#include <genesis.h>

void updateVisibleTiles(TileMap *map, u16 x, u16 y);

void MapCB(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size);