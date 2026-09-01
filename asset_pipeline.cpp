#include "PPU466.hpp"
#include "load_save_png.hpp"
#include "read_write_chunk.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <vector>

// hardcoded palettes lol
static std::array<PPU466::Palette, 8> const palettes = {{
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0xB2, 0xAE, 0xB3, 0xff), glm::u8vec4(0xCA, 0x6F, 0x2D, 0xff), glm::u8vec4(0x00, 0x00, 0x00, 0xff)}}, // swan face      -- 0:clear 1:B2AEB3 head 2:CA6F2D beak 3:000000 eye
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0xB2, 0xAE, 0xB3, 0xff), glm::u8vec4(0xCA, 0x57, 0x2D, 0xff), glm::u8vec4(0x89, 0x89, 0x89, 0xff)}}, // swan body / HUD -- 0:clear 1:B2AEB3 light 2:CA572D legs 3:898989 dark
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0xCA, 0xAD, 0x2D, 0xff), glm::u8vec4(0xCA, 0x57, 0x2D, 0xff), glm::u8vec4(0x00, 0x00, 0x00, 0xff)}}, // ducklings      -- 0:clear 1:CAAD2D body 2:CA572D beak 3:000000 eye
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0x39, 0x3A, 0x3E, 0xff), glm::u8vec4(0x4B, 0x4C, 0x52, 0xff), glm::u8vec4(0xDD, 0xD9, 0xCE, 0xff)}}, // road           -- 0:clear 1:393A3E asphalt 2:4B4C52 speckle 3:DDD9CE dash
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0x55, 0xAE, 0x2F, 0xff), glm::u8vec4(0x4C, 0x9C, 0x2B, 0xff), glm::u8vec4(0x00, 0x00, 0x00, 0x00)}}, // grass          -- 0:clear 1:55AE2F light 2:4C9C2B dark 3:unused
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0xB5, 0x31, 0x31, 0xff), glm::u8vec4(0x72, 0x23, 0x23, 0xff), glm::u8vec4(0x20, 0x20, 0x22, 0xff)}}, // cars (red)     -- 0:clear 1:B53131 body 2:722323 shadow 3:202022 outline
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0xB9, 0xC9, 0x39, 0xff), glm::u8vec4(0x5B, 0x91, 0x24, 0xff), glm::u8vec4(0x20, 0x20, 0x22, 0xff)}}, // cars (yellow/green) -- 0:clear 1:B9C939 body 2:5B9124 shadow 3:202022 outline
    {{glm::u8vec4(0x00, 0x00, 0x00, 0x00), glm::u8vec4(0x2F, 0x1E, 0xA4, 0xff), glm::u8vec4(0x30, 0x1C, 0x66, 0xff), glm::u8vec4(0x20, 0x20, 0x22, 0xff)}}, // cars(blue)     -- 0:clear 1:2F1EA4 body 2:301C66 shadow 3:202022 outline
}};

// design decision: just hardcode my sprites with 4 colors each:
// transparency represents the transparent pixels
// red represents the pallete's second idx
// green represents the palette's third idx
// any other color without max values (like black) represents the palette's fourth idx
static uint8_t color_idx(glm::u8vec4 c)
{
    if (c.a == 0)
        return 0;
    if (c.r == 0xff)
        return 1;
    if (c.g == 0xff)
        return 2;
    return 3;
}

int main()
{
    std::string sprites_file = "sprites/sprites.png";
    std::string out_file = "dist/assets.blob";

    glm::uvec2 size;
    std::vector<glm::u8vec4> sheet;
    load_png(sprites_file, &size, &sheet, LowerLeftOrigin);

    // convert to tiles
    std::vector<PPU466::Tile> tiles(256);

    for (uint32_t ty = 0; ty < 16; ++ty)
    {
        for (uint32_t tx = 0; tx < 16; ++tx)
        {
            uint32_t idx = tx + 16 * ty;
            uint32_t base_x = tx * 8;
            uint32_t base_y = (15 - ty) * 8;

            for (uint32_t y = 0; y < 8; ++y)
            {
                uint8_t bit0 = 0;
                uint8_t bit1 = 0;
                for (uint32_t x = 0; x < 8; ++x)
                {
                    uint8_t ci = color_idx(sheet[(base_x + x) + size.x * (base_y + y)]);
                    if (ci & 1)
                        bit0 |= (1 << x);
                    if (ci & 2)
                        bit1 |= (1 << x);
                }
                tiles[idx].bit0[y] = bit0;
                tiles[idx].bit1[y] = bit1;
            }
        }
    }

    std::ofstream out(out_file, std::ios::binary);
    std::vector<PPU466::Palette> palette_vec(palettes.begin(), palettes.end());
    write_chunk("tile", tiles, &out);
    write_chunk("pal0", palette_vec, &out);

    std::cout << "wrote " << out_file << std::endl;

    return 0;
}
