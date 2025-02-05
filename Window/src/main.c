#include <genesis.h>
#include <resources.h>

// Window trick
vu8 TEXTBOX_H = 8;
vu16 TEXTBOX_Y = 0;
vs8 TEXTBOX_VY = 1;
vu8 TextboxIntStep;

HINTERRUPT_CALLBACK Window_HINT_TOP()
{

    if(TextboxIntStep == 1)
    {
        VDP_setWindowOnTop(TEXTBOX_Y + TEXTBOX_H);     // Turn on an 8 tile high textbox right here
        VDP_setHInterrupt(false);                      // Turn off HINT and let nature take it's course
    }
    ++TextboxIntStep;    
}

HINTERRUPT_CALLBACK Window_HINT_LOW()
{
    if(TextboxIntStep == 0)
    {     
        VDP_setWindowOnTop(TEXTBOX_Y + TEXTBOX_H);             
    }
    ++TextboxIntStep;    
}

void Window_VINT()
{
    TextboxIntStep = 0;

    VDP_setHIntCounter(0);
    VDP_setHInterrupt(true);    

    if(TEXTBOX_Y == 0)  // Pinned to top
    {
        VDP_setHInterrupt(false);
        VDP_setWindowOnTop(TEXTBOX_H);
    }
    else if(TEXTBOX_Y == 28)    // Pinned to bottom
    {
        VDP_setHInterrupt(false);
        VDP_setWindowOnBottom(28 - TEXTBOX_H);
    }
    else if(TEXTBOX_Y < 14)     // top half of screen
    {
        SYS_setHIntCallback(Window_HINT_TOP);
        VDP_setHIntCounter(TEXTBOX_Y * 8);
        VDP_setWindowOff();
        //VDP_setWindowOnBottom(28 - (TEXTBOX_Y + TEXTBOX_H));
    }
    else    // bottom half of screen
    {
        SYS_setHIntCallback(Window_HINT_LOW);
        VDP_setHIntCounter((TEXTBOX_Y + TEXTBOX_H) * 8);  
        VDP_setWindowOnBottom(28 - TEXTBOX_Y);   
    }
}

// 

int main(bool resetType) 
{
    if (!resetType)
        SYS_hardReset();

    // Setup
    VDP_setPlaneSize(64, 32, true);
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    // Draw some sort of interesting background
    uint16_t VRAM = 1;
    uint16_t vramBG = VRAM;
    VRAM += VDP_loadTileSet(&imgBG, VRAM, DMA);

    for(uint8_t x = 0; x < 64; ++x)
    {
        for(uint8_t y = 0; y < 32; ++y)
        {
            // slow... but fine for a demo
            VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, false, y & 1, x & 1, vramBG), x, y, 1, 1);
            VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL1, (x >> 1), y & 1, (x + 1) & 1, vramBG), x, y, 1, 1);
        }
    }

    // Draw textbox
    PAL_setColors(PAL2 * 16, imgTextbox.palette->data, 16, DMA);
    
    for(uint8_t i = 0; i < 30; i += 8)
        VDP_drawImageEx(WINDOW, &imgTextbox, TILE_ATTR_FULL(PAL2, true, false, false, VRAM), 0, i, false, true);
    VRAM += imgTextbox.tileset->numTile;
    
    // Final setup
    DMA_waitCompletion();
    SPR_initEx(TILE_FONT_INDEX - VRAM); // cool sgdk trick - fully count background tiles and call like this maximise sprite memory

    // Setup interrupts
    SYS_setHIntCallback(NULL);          VDP_setHInterrupt(true);    VDP_setHIntCounter(0);  
    SYS_setVIntCallback(Window_VINT);   VDP_setVInterrupt(true);
    
    int i = 0;
    while(1)
    {      
        ++i;
        // Make the background do something interesting 
        VDP_setVerticalScroll(BG_A, i);
        VDP_setHorizontalScroll(BG_B, i);

        // Move the window
        if(i % 4 == 0)
        {
            TEXTBOX_Y += TEXTBOX_VY;
            if(TEXTBOX_Y == 0 || TEXTBOX_Y == 20)
                TEXTBOX_VY = TEXTBOX_VY * -1;
        }

        // etc
        SPR_update();
        SYS_doVBlankProcess();        
    }
}