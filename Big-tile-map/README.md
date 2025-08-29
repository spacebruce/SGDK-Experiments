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
When setting up a SGDK map, call **tileCache_init**, it will handle it's own init and runtime.
```
    bigmap = MAP_create(&mBigMap, BG_B, 0);  // Same as always, use tile 0
    vram = tileCache_init(bigmap, &tsBigMap, 300, 0);  // returns a bumped tile pointer for easy integration
```

The rest is handled automatically! Call tileCache_free() when you're done. 

## Functions

uint16_t tileCache_init(uint16_t vram, const TileSet* ts, uint16_t activeTiles);
- map : The relevant Map handler pointer
- ts : The tileset being used
- activeTiles : how tiles are being used in VRAM
- reserve : how many tiles are static
- Returns : vram + activeTiles
  
void tileCache_free();
- Clears out tileCache engine

### Debug functions
void tileCache_print();
- Prints to BG A some debug stats
uint16_t tileCache_getUsage();
- Returns the max number of active tiles needed for the current map, lets you figure out if you need to shrink or grow the settings

## Notes
All "static tiles" are pulled from the start of the tileset, so setting it to 30 will upload the first 30 tiles of the set and then leave them unmanaged so you can do your own tricks on them.
The cache automatically switches off if you give it more tiles than needed. e.g. setting 512 allocated tiles on a set of only 300 - all will be uploaded and vram pointer bumped by 300, the tilecache is not switched on.

## Demo video: 
https://github.com/user-attachments/assets/895e24df-157a-4b0d-b77a-968cdac8bc21
