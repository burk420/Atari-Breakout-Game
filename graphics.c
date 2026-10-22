/*
 * graphics.c
 *
 */

#include "graphics.h"

void Initialize_Graphics(Graphics_Context * context) {
    // Set our screen orientation
    Crystalfontz128x128_SetOrientation(0);

    // Initialize our graphics context
    Graphics_initContext(context, &g_sCrystalfontz128x128);

    // Set our default background and foreground colors
    Graphics_setBackgroundColor(context, GRAPHICS_COLOR_BLACK);
    Graphics_setForegroundColor(context, GRAPHICS_COLOR_WHITE);

    // Establish our default font
    GrContextFontSet(context, &g_sFontFixed6x8);

    // Clear the screen
    Graphics_clearDisplay(context);

    // Set the display's backlight to high
    LCD_BL_DIR |= LCD_BL_BIT;
    LCD_BL_OUT |= LCD_BL_BIT;
}

void Draw_Playspace(Graphics_Context *context, Graphics_Rectangle *blocks, int32_t *colors, char *bindings, int numBlocks, int lives, int levelNumber) {
    unsigned char i;
    char levelBuffer[9];

    // Clear the screen
    Graphics_setBackgroundColor(context, GRAPHICS_COLOR_BLACK);
    Graphics_setForegroundColor(context, GRAPHICS_COLOR_WHITE);
    Graphics_clearDisplay(context);

    // Draw the UI spacer
    Graphics_drawLineH(context, 0, 127, 118);

    // Draw our level label
    sprintf(levelBuffer, "Level %02d", levelNumber + 1);
    Graphics_drawStringCentered(context, levelBuffer, AUTO_STRING_LENGTH, 64, 123, OPAQUE_TEXT);

    // Draw the lives
    for(i = 0; i < lives; i++) {
        Graphics_fillCircle(context, (i*6)+4, 123, BALL_RADIUS);
    }

    // Draw our set of blocks
    Graphics_setForegroundColor(context, GRAPHICS_COLOR_BLUE);
    for(i = 0; i < numBlocks; i++) {
        if(IsNullBlock(&blocks[i])) continue;

        // Get the color
        char bindingIndex = bindings[i];
        Graphics_setForegroundColor(context, colors[bindingIndex]);
        Graphics_fillRectangle(context, &blocks[i]);
    }
}

void Draw_Paddle(Graphics_Context *context, int paddle_x, int32_t color) {
    // Draw the initial paddle
    Graphics_setForegroundColor(context, color);
    Graphics_Rectangle paddle = {paddle_x, PADDLE_Y, paddle_x + PADDLE_WIDTH, PADDLE_Y + PADDLE_HEIGHT};
    Graphics_fillRectangle(context, &paddle);
}

void Draw_PaddleMove(Graphics_Context *context, int oldX, int newX) {
    int oldR = oldX + PADDLE_WIDTH, newR = newX + PADDLE_WIDTH;
    Graphics_Rectangle r;

    if(oldX == newX) return;

    if(newX > oldX) {
        // Moving right: clear the strip the paddle left, paint the strip it entered
        r.xMin = oldX;  r.xMax = (oldR < newX - 1) ? oldR : newX - 1;
        r.yMin = PADDLE_Y; r.yMax = PADDLE_Y + PADDLE_HEIGHT;
        Graphics_setForegroundColor(context, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(context, &r);

        r.xMin = (newX > oldR + 1) ? newX : oldR + 1;  r.xMax = newR;
        Graphics_setForegroundColor(context, GRAPHICS_COLOR_RED);
        Graphics_fillRectangle(context, &r);
    } else {
        // Moving left
        r.xMin = (oldX > newR + 1) ? oldX : newR + 1;  r.xMax = oldR;
        r.yMin = PADDLE_Y; r.yMax = PADDLE_Y + PADDLE_HEIGHT;
        Graphics_setForegroundColor(context, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(context, &r);

        r.xMin = newX;  r.xMax = (newR < oldX - 1) ? newR : oldX - 1;
        Graphics_setForegroundColor(context, GRAPHICS_COLOR_RED);
        Graphics_fillRectangle(context, &r);
    }
}

void Draw_BlocksInRect(Graphics_Context *context, Graphics_Rectangle *blocks, int32_t *colors, char *bindings, int numBlocks, Graphics_Rectangle *rect) {
    int i;
    for(i = 0; i < numBlocks; i++) {
        if(IsNullBlock(&blocks[i])) continue;
        if(blocks[i].xMin <= rect->xMax && blocks[i].xMax >= rect->xMin &&
           blocks[i].yMin <= rect->yMax && blocks[i].yMax >= rect->yMin) {
            Graphics_setForegroundColor(context, colors[(int)bindings[i]]);
            Graphics_fillRectangle(context, &blocks[i]);
        }
    }
}

void Draw_Separator(Graphics_Context *context, int x1, int x2) {
    if(x1 < 0) x1 = 0;
    if(x2 > 127) x2 = 127;
    Graphics_setForegroundColor(context, GRAPHICS_COLOR_WHITE);
    Graphics_drawLineH(context, x1, x2, 118);
}

void Draw_Ball(Graphics_Context *context, int ball_x, int ball_y, int32_t color) {
    Graphics_setForegroundColor(context, color);
    Graphics_fillCircle(context, ball_x, ball_y, BALL_RADIUS);
}

void Draw_EdgedBox(Graphics_Context *context, Graphics_Rectangle *rect, int inset, int32_t backgroundColor, int32_t foregroundColor) {
    Graphics_Rectangle insetRect = {rect->xMin + inset, rect->yMin + inset, rect->xMax + inset, rect->yMax + inset};

    Graphics_setForegroundColor(context, backgroundColor);
    Graphics_fillRectangle(context, rect);

    Graphics_setForegroundColor(context, foregroundColor);
    Graphics_drawRectangle(context, &insetRect);
}

void Draw_Points(Graphics_Context *context, unsigned int points) {
    char buff[10];
    Graphics_setForegroundColor(context, GRAPHICS_COLOR_WHITE);
    sprintf(buff, "%d pts", points);

    Graphics_drawStringCentered(context, buff, AUTO_STRING_LENGTH, 64, 75, TRANSPARENT_TEXT);
}

void Draw_ModalBox(Graphics_Context *context, const char* line1, const char* line2, char showPoints, int points) {
    Graphics_Rectangle rect = { 10, 20, 110, 90 };
    Draw_EdgedBox(context, &rect, 2, GRAPHICS_COLOR_BLACK, GRAPHICS_COLOR_BLUE);

    Graphics_setForegroundColor(context, GRAPHICS_COLOR_WHITE);
    Graphics_drawStringCentered(context, line1, AUTO_STRING_LENGTH, 64, 35, OPAQUE_TEXT);
    Graphics_drawStringCentered(context, line2, AUTO_STRING_LENGTH, 64, 55, OPAQUE_TEXT);
    if(showPoints) Draw_Points(context, points);
}
