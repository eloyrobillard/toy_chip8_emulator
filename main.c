#include <stdio.h>
#include "chip8.h"
#include "raylib.h"

#define SCALE 10

int main() {
    char *filename = "3-corax+.ch8";
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("%s: no such file\n", filename);
        return 0;
    }

    fpos_t fSize;
    fseek(f, 0, SEEK_END);
    fgetpos(f, &fSize);
    rewind(f);
    chip8* ch8 = chip8_init();
    fread(&ch8->memory[ch8->PC], 1, fSize.__pos, f);

    InitWindow(WIDTH*SCALE, HEIGHT*SCALE, "Chip 8");
    while (!WindowShouldClose())
    {
        opcode(ch8);
        BeginDrawing();
            ClearBackground(RAYWHITE);
            for (int posX = 0; posX<WIDTH; posX++)
            {
                for (int posY = 0; posY<HEIGHT; posY++) 
                {
                    if (ch8->draw && ch8->gfx[posY][posX]) {
                        DrawRectangle(posX*SCALE, posY*SCALE, SCALE, SCALE, PINK);
                    }   
                }
            }
        EndDrawing();
    }

    CloseWindow();
    fclose(f);
    return 0;
}