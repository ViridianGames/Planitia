#include "PlanitiaGuiLegacy.h"

#include "../Geist/Source/Globals.h"
#include "../Geist/Source/Gui.h"
#include "../Geist/Source/GuiElements.h"
#include "../Geist/Source/Primitives.h"
#include "../Geist/Source/ResourceManager.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace {

std::string NormalizeLegacyPath(std::string path)
{
    for (char& ch : path)
    {
        if (ch == '\\')
            ch = '/';
    }
    return path;
}

std::string ReadLegacyStringField(std::stringstream& lineData)
{
    std::string remainder;
    std::getline(lineData, remainder, '\r');
    if (remainder.empty())
        std::getline(lineData, remainder);

    while (!remainder.empty() && (remainder[0] == ' ' || remainder[0] == '\t'))
        remainder.erase(0, 1);

    while (!remainder.empty() && (remainder.back() == '\n' || remainder.back() == '\r'))
        remainder.pop_back();

    return remainder;
}

Texture* LoadLegacyTexture(const std::string& path)
{
    const std::string normalized = NormalizeLegacyPath(path);
    if (!g_ResourceManager->DoesFileExist(normalized))
        return nullptr;
    return g_ResourceManager->GetTexture(normalized);
}

enum LegacyGuiType
{
    LEGACY_BUTTON = 0,
    LEGACY_SCROLLBAR,
    LEGACY_TEXTAREA,
    LEGACY_RADIOBUTTON,
    LEGACY_BITMAP,
    LEGACY_PANEL,
    LEGACY_TEXTLABEL,
};

} // namespace

void LoadGuiLegacyFile(Gui& gui, const std::string& fileName)
{
    gui.m_GuiElementList.clear();

    std::ifstream instream(fileName);
    if (instream.fail())
        return;

    int guiX = 0;
    int guiY = 0;
    instream >> guiX;
    instream >> guiY;
    gui.m_Pos.x = float(guiX);
    gui.m_Pos.y = float(guiY);

    std::string line;
    std::getline(instream, line);
    std::getline(instream, line); // legacy font config line (ignored)

    while (!instream.eof())
    {
        std::getline(instream, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::stringstream lineData;
        lineData << line;

        int type = -1;
        lineData >> type;
        switch (type)
        {
        case LEGACY_BUTTON:
        {
            int buttonId = 0;
            int active = 0;
            int group = 0;
            int tileX = 0;
            int tileY = 0;
            int posX = 0;
            int posY = 0;
            int width = 0;
            int height = 0;
            lineData >> buttonId >> active >> group >> tileX >> tileY >> posX >> posY >> width >> height;
            const std::string bitmap = ReadLegacyStringField(lineData);
            Texture* texture = LoadLegacyTexture(bitmap);
            if (texture)
            {
                gui.AddIconButton(buttonId, texture, posX, posY, tileX, tileY, width, height,
                    "", gui.m_Font.get(), Color{ 255, 255, 255, 255 }, 1.0f, group, active != 0);
            }
            break;
        }

        case LEGACY_SCROLLBAR:
        {
            int id = 0;
            int active = 0;
            int group = 0;
            int posX = 0;
            int posY = 0;
            int width = 0;
            int height = 0;
            int spurLocation = 0;
            int r = 255;
            int g = 255;
            int b = 255;
            int a = 255;
            lineData >> id >> active >> group >> posX >> posY >> width >> height >> spurLocation >> r >> g >> b >> a;
            GuiScrollBar* scrollbar = gui.AddScrollBar(id, height, posX, posY, width, height, true,
                Color{ static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b), static_cast<unsigned char>(a) },
                Color{ 0, 0, 0, 0 }, group, active != 0);
            if (scrollbar)
                scrollbar->m_SpurLocation = spurLocation;
            break;
        }

        case LEGACY_TEXTAREA:
        {
            int id = 0;
            int active = 0;
            int group = 0;
            int posX = 0;
            int posY = 0;
            int width = 0;
            int height = 0;
            int r = 255;
            int g = 255;
            int b = 255;
            int a = 255;
            lineData >> id >> active >> group >> posX >> posY >> width >> height >> r >> g >> b >> a;
            const std::string text = ReadLegacyStringField(lineData);
            gui.AddTextArea(id, gui.m_Font.get(), text, posX, posY, width, height,
                Color{ static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b), static_cast<unsigned char>(a) },
                GuiTextArea::LEFT, group, active != 0);
            break;
        }

        case LEGACY_RADIOBUTTON:
        {
            int buttonId = 0;
            int active = 0;
            int group = 0;
            int tileX = 0;
            int tileY = 0;
            int posX = 0;
            int posY = 0;
            int width = 0;
            int height = 0;
            int radioButtonGroup = 0;
            int set = 0;
            lineData >> buttonId >> active >> group >> tileX >> tileY >> posX >> posY >> width >> height
                >> radioButtonGroup >> set;
            (void)group;
            const std::string bitmap = ReadLegacyStringField(lineData);
            Texture* texture = LoadLegacyTexture(bitmap);
            if (texture)
            {
                auto selected = std::make_shared<Sprite>(texture, tileX, tileY + height, width, height);
                auto unselected = std::make_shared<Sprite>(texture, tileX, tileY, width, height);
                auto hovered = std::make_shared<Sprite>(texture, tileX + width, tileY, width, height);
                GuiRadioButton* radioButton = gui.AddRadioButton(buttonId, posX, posY, selected, unselected, hovered,
                    1.0f, 1.0f, Color{ 255, 255, 255, 255 }, radioButtonGroup, active != 0);
                if (radioButton)
                    radioButton->m_Selected = (set != 0);
            }
            break;
        }

        case LEGACY_BITMAP:
        {
            int id = 0;
            int active = 0;
            int group = 0;
            int posX = 0;
            int posY = 0;
            lineData >> id >> active >> group >> posX >> posY;
            const std::string bitmap = ReadLegacyStringField(lineData);
            Texture* texture = LoadLegacyTexture(bitmap);
            if (texture)
            {
                auto sprite = std::make_shared<Sprite>(texture, 0, 0, texture->width, texture->height);
                gui.AddSprite(id, posX, posY, sprite, 1.0f, 1.0f, Color{ 255, 255, 255, 255 }, group, active != 0);
            }
            break;
        }

        case LEGACY_PANEL:
        {
            int id = 0;
            int active = 0;
            int group = 0;
            int posX = 0;
            int posY = 0;
            int width = 0;
            int height = 0;
            int filled = 0;
            int r = 0;
            int g = 0;
            int b = 0;
            int a = 255;
            lineData >> id >> active >> group >> posX >> posY >> width >> height >> filled >> r >> g >> b >> a;
            gui.AddPanel(id, posX, posY, width, height,
                Color{ static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b), static_cast<unsigned char>(a) },
                filled != 0, group, active != 0);
            break;
        }

        case LEGACY_TEXTLABEL:
        {
            int id = 0;
            int active = 0;
            int group = 0;
            int posX = 0;
            int posY = 0;
            int r = 255;
            int g = 255;
            int b = 255;
            int a = 255;
            lineData >> id >> active >> group >> posX >> posY >> r >> g >> b >> a;
            const std::string text = ReadLegacyStringField(lineData);
            gui.AddTextArea(id, gui.m_Font.get(), text, posX, posY, 0, 0,
                Color{ static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b), static_cast<unsigned char>(a) },
                GuiTextArea::LEFT, group, active != 0);
            break;
        }

        default:
            break;
        }
    }
}