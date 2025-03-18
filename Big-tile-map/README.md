# Big virtual scrolling tilemap

A reasonably succesful attempt at shrinking VRAM usage by only allocating what's actually needed to the screen, allowing very big and graphically diverse maps.
Uses more CPU and RAM (not VRAM) than I'm super comfy with, but does seem to do it's job well when given adequate resources.

## Limitations
- Uses _a lot_ but possibly not unbearable amount of memory, scales up with how many tiles exist in the map (MapTiles) and how many are visible in the view (ActiveTiles)
-  - Approximately : 2700 + (ActiveTiles × 6) + (MapTiles * 2) Bytes
   - a 1251 tile map (1251 * 2) with 320 tiles visible (320 * 6) = 4422 bytes + 2270 = 7122B ~ 7KB of RAM
- Currently hijacks the MAP type, and has the same 2048 maximum tile limitations as that
- Is _not_ compatible with compressed tilesets. Must be set to NONE. :( 

## Usage
When setting up a SGDK map, call **tileCache_init** and hook in the **tileCache_callback** 
```
    bigmap = MAP_create(&mBigMap, BG_B, 0);  // Same as always, use tile 0
    vram = tileCache_init(vram, &tsBigMap, 300);  // returns a bumped tile pointer for easy integration
    MAP_setDataPatchCallback(bigmap, tileCache_callback);  // Magic bit: Makes the Map handler call the cache when moving around
```
The rest is handled automatically! Call tileCache_free() when you're done. 

## Functions
uint16_t tileCache_init(uint16_t vram, const TileSet* ts, uint16_t activeTiles);
- vram : Where to allocate
- ts : The tileset being used
- activeTiles : how tiles are being used in VRAM
- Returns : vram + activeTiles
  
void tileCache_free();
- Clears out tileCache engine

void tileCache_callback(Map *map, u16 *buf, u16 x, u16 y, MapUpdateType updateType, u16 size);
- Don't call this directly, set using MAP_setDataPatchCallback(bigmap, tileCache_callback);

### Debug functions
void tileCache_print();
- Prints to BG A some debug stats
uint16_t tileCache_getUsage();
- Returns the max number of active tiles needed for the current map, lets you figure out if you need to shrink or grow the settings

## Demo video: 
https://github.com/user-attachments/assets/895e24df-157a-4b0d-b77a-968cdac8bc21
