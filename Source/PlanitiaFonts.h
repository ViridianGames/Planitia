#ifndef PLANITIA_FONTS_H
#define PLANITIA_FONTS_H

#include "raylib.h"
#include <string>

extern Font g_PlanitiaFont;
extern Font g_PlanitiaCompactFont;

// Sizes are in design-resolution pixels (authored for 1600x900 layouts).
constexpr float PLANITIA_FONT_SIZE = 28.0f;
constexpr float PLANITIA_COMPACT_FONT_SIZE = 20.0f;

void InitPlanitiaFonts();
void ShutdownPlanitiaFonts();

float PlanitiaUIScaleX();
float PlanitiaUIScaleY();

float PlanitiaMeasureText(const Font& font, float logicalSize, const std::string& text);
void PlanitiaDrawText(const Font& font, float logicalSize, const std::string& text,
    float logicalX, float logicalY, Color color);
void PlanitiaDrawText(const Font& font, float logicalSize, const std::string& text,
    float logicalX, float logicalY, int r, int g, int b, int a = 255);
void PlanitiaDrawTextCentered(const Font& font, float logicalSize, const std::string& text,
    float logicalCenterX, float logicalY, Color color);

#endif