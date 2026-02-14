#include <libreborn/patch.h>

#include <symbols/Font.h>
#include <symbols/Tesselator.h>
#include <symbols/Textures.h>

#include <GLES/gl.h>

#include <mods/tesselator/tesselator.h>

#include "internal.h"

#define COLOR_CHAR 0x15


#ifdef LAME_COLORS
// Old and harsh colors
uint color_table[16] = {
    0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
    0xaa0000, 0xaa00aa, 0xffaa00, 0xaaaaaa,
    0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
    0xff5555, 0xff55ff, 0xffff55, 0xffffff
};
#endif
#ifndef LAME_COLORS
// New, cozy, dye-based colors
uint text_color_table[16] = {
    0x272727, 0x2d44aa, 0x094816, 0x30c2ca,
    0xdb161a, 0x9730c3, 0x583337, 0xadaba8,
    0x525557, 0x8bd7ff, 0x7bd53a, 0xf68008,
    0xf9aecd, 0xd344b1, 0xf9f618, 0xffffff
};
#endif


// TBR add this please!
void media_glCallList(GLuint list) {
    media_glCallLists(1, GL_UNSIGNED_INT, &list);
}

// Color index decoding
inline int char_to_hex_int(char c) {
    if (c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Create Display lists
static void Font_init_injection(Font_init_t original, Font *self, Options *options) {
    // Call Original Method
    original(self, options);

    // Generate Lists
    constexpr int character_count = sizeof(self->character_widths) / sizeof(int);
    self->display_lists = media_glGenLists(character_count);
    CustomTesselator &advanced_t = advanced_tesselator_get();
    advanced_t.are_vertices_flat = true;
    for (int i = 0; i < character_count; i++) {
        media_glNewList(self->display_lists + i, GL_COMPILE);
        Tesselator &t = Tesselator::instance;
        t.begin(GL_QUADS);
        self->buildChar(uchar(i), 0, 0);
        t.draw();
        media_glTranslatef(GLfloat(self->character_widths[i]), 0, 0);
        media_glEndList();
    }
    advanced_t.are_vertices_flat = false;
}

// Custom Rendering
static void Font_drawSlow_injection(Font_drawSlow_t original, Font *self, const char *text, const float x, float y, uint color, const bool is_shadow) {
    // If this is part of a batched render, do the old/slow method.
    const Tesselator &t = Tesselator::instance;
    if (t.void_begin_end) {
        original(self, text, x, y, color, is_shadow);
        return;
    }

    // Check Text
    if (!text || text[0] == '\0') {
        return;
    }

    // Darken Text
    if (is_shadow) {
        const uint val = color & 0xff000000;
        color = (color & 0xfcfcfc) >> 2;
        color += val;
    }

    // Bind Texture
    self->textures->loadAndBindTexture(self->texture_name);

    // Set Color
    float a = float((color >> 24) & 0xff) / 255.0f;
    if (a == 0) {
        a = 1;
    }
    const float r = float((color >> 16) & 0xff) / 255.0f;
    const float g = float((color >> 8) & 0xff) / 255.0f;
    const float b = float((color) & 0xff) / 255.0f;
    media_glColor4f(r, g, b, a);

    media_glPushMatrix();
    media_glTranslatef(x, y, 0);

    // Render Lines
    size_t position = 0;
    bool has_another_line = true;
    while (has_another_line) {
        // Handle Newline
        if (position > 0) {
            y += float(self->line_height);
        }
        has_another_line = false;

        // Read Line
        static constexpr int max_line_size = 512;
        int line_size = 0;
        while (text[position] != '\0') {
            // Read Character
            char c = text[position];
            position++;
            // Handle Character
            if (c == '\n') {
                // Found Newline
                has_another_line = true;
                break;
            }
            if (c == COLOR_CHAR) {
                // Custom colors/rendering
                c = text[position];
                if (c == '\0') {
                    media_glCallList(self->display_lists + uchar(COLOR_CHAR));
                    break;
                }
                position++;
                
                switch (c) {
                    case '\n':
                        break;
                    case 'r':
                        // Reset
                        media_glColor4f(r, g, b, a);
                        break;
                    case COLOR_CHAR:
                        // Escape the character
                        media_glCallList(self->display_lists + uchar(COLOR_CHAR));
                        break;
                    default:
                        int new_color_id = char_to_hex_int(c);
                        if (new_color_id >= 0) {
                            // Determine color
                            uint new_color = text_color_table[new_color_id];
                            float nr = float((new_color >> 16) & 0xff) / 255.0f;
                            float ng = float((new_color >> 8) & 0xff) / 255.0f;
                            float nb = float((new_color) & 0xff) / 255.0f;
                            
                            if (is_shadow) {
                                nr *= 0.25;
                                ng *= 0.25;
                                nb *= 0.25;
                            }

                            media_glColor4f(nr, ng, nb, a);
                        } else {
                            // Invalid color, render underlying text
                            media_glCallList(self->display_lists + uchar(COLOR_CHAR));
                            media_glCallList(self->display_lists + uchar(c));
                        }
                        break;
                }

                continue;
            }
            if (line_size < max_line_size) {
                // Render
                media_glCallList(self->display_lists + uchar(c));
            }
        }
        
        media_glPopMatrix();
    }

    // Reset Color
    media_glColor4f(1, 1, 1, 1);
}

static int Font_width_injection(Font_width_t original, Font *self, const std::string &line) {
    int minimum = 0;
    int width = 0;

    for (int i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == COLOR_CHAR && i + 1 < line.size()) {
            char n = line[i + 1];
            if (n == 'r' || (char_to_hex_int(n) != -1)) i++;
            if (n == COLOR_CHAR) {
                width += self->character_widths[c];
            }
        } else if (c == '\n') {
            if (minimum < width) {
                minimum = width;
            }
            width = 0;
        } else {
            width += self->character_widths[c];
        }
    }
    if (width < minimum) {
        width = minimum;
    }
    return width;
}

// Init
__attribute__((constructor)) static void init_mod() {
    advanced_tesselator_enable();
    overwrite_calls(Font_init, Font_init_injection);
    overwrite_calls(Font_drawSlow, Font_drawSlow_injection);
    overwrite_calls(Font_width, Font_width_injection);
}