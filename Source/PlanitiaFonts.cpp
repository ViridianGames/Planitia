#include "PlanitiaFonts.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaScene.h"
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"

#include "rlgl.h"

Font g_PlanitiaFont{};
Font g_PlanitiaCompactFont{};

void InitPlanitiaFonts()
{
    g_PlanitiaFont = LoadFont("Fonts/softsquare.ttf");
    g_PlanitiaCompactFont = LoadFont("Fonts/littleleague.ttf");

    if (g_PlanitiaFont.texture.id != 0)
        SetTextureFilter(g_PlanitiaFont.texture, TEXTURE_FILTER_POINT);
    if (g_PlanitiaCompactFont.texture.id != 0)
        SetTextureFilter(g_PlanitiaCompactFont.texture, TEXTURE_FILTER_POINT);
}

void ShutdownPlanitiaFonts()
{
    if (g_PlanitiaFont.texture.id != 0)
        UnloadFont(g_PlanitiaFont);
    g_PlanitiaFont = {};

    if (g_PlanitiaCompactFont.texture.id != 0)
        UnloadFont(g_PlanitiaCompactFont);
    g_PlanitiaCompactFont = {};
}

float PlanitiaUIScaleX()
{
    if (!gp_Scene) return 1.0f;
    return gp_Scene->UIScaleX();
}

float PlanitiaUIScaleY()
{
    if (!gp_Scene) return 1.0f;
    return gp_Scene->UIScaleY();
}

float PlanitiaMeasureText(const Font& font, float logicalSize, const std::string& text)
{
    const float scaledSize = logicalSize * PlanitiaUIScaleY();
    return MeasureTextEx(font, text.c_str(), scaledSize, 1).x / PlanitiaUIScaleX();
}

void PlanitiaDrawText(const Font& font, float logicalSize, const std::string& text,
    float logicalX, float logicalY, Color color)
{
    const float sx = PlanitiaUIScaleX();
    const float sy = PlanitiaUIScaleY();
    rlDrawRenderBatchActive();
    DrawTextEx(font, text.c_str(),
        {logicalX * sx, logicalY * sy},
        logicalSize * sy, 1, color);
}

void PlanitiaDrawText(const Font& font, float logicalSize, const std::string& text,
    float logicalX, float logicalY, int r, int g, int b, int a)
{
    PlanitiaDrawText(font, logicalSize, text, logicalX, logicalY,
        Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
              static_cast<unsigned char>(b), static_cast<unsigned char>(a)});
}

void PlanitiaDrawTextCentered(const Font& font, float logicalSize, const std::string& text,
    float logicalCenterX, float logicalY, Color color)
{
    const float width = PlanitiaMeasureText(font, logicalSize, text);
    PlanitiaDrawText(font, logicalSize, text, logicalCenterX - width * 0.5f, logicalY, color);
}