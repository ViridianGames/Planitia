///////////////////////////////////////////////////////////////////////////
//
// Name:     GUI.H
// Author:   Anthony Salter
// Date:     10/22/05
// Purpose:  Graphic User Interface Object.
//
///////////////////////////////////////////////////////////////////////////

//  About the simplest GUI system you will ever see.  Only handles left-clicks;
//  it's basically just a list of regions.  On every click the list is checked
//  to see if any of the regions has been clicked in; if they have, the system
//  runs the appropriate code.

// REVAMP:  Okay, what do we need here?

//  We need to be able to load GUIs from files.
//  We need to seriously do something about how you handle two-press
//  interactions (that is, you press something on the GUI and then you
//  press something in the game world to drop an object, say).

//  We might be able to handle that by, on press, looking at what USED to
//  be the active button (that is, the last active button).  If we're
//  currently clicking in the world and the last button was "Talk", say, 
//  then we check to see if there is an NPC under the mouse cursor now, and
//  if so, we initiate the "Talk" action on that NPC.  So it all stays
//  here, instead of getting munged up in the input code.

//  That would probably work.

//  GUIs can be declared and used in any subsystem, but will mostly be used
//  inside states.  A state will have a member GUI that it can easily
//  draw and check interactions on.

//  For simplicity's sake, we are going to assume that all buttons are on a
//  single tile sheet.  Button tiles should be arranged in square quads.
//  We will assume that the upper-left tile defines the normal state, the
//  upper-right tile defines the "hot" state, the lower-left tile defines
//  the "clicked" state, and the lower-right tile defines the "inactive"
//  state (greyed out).

#pragma warning (disable : 4786)

#ifndef PLANITIA_GUI_H
#define PLANITIA_GUI_H

#include "PlanitiaObject.h"
#include "PlanitiaPrimitives.h"
#include "raylib.h"
#include <list>
#include <vector>

enum GuiElements
{
	//  Actual working elements
	GUI_BUTTON = 0,
	GUI_SCROLLBAR,
	GUI_TEXTAREA,
	GUI_RADIOBUTTON,

	//  Cosmetics
	GUI_BITMAP,
	GUI_PANEL,
	GUI_TEXTLABEL,

	GUI_LAST
};

class GuiElement
{
public:
	GuiElement(){ m_Visible = true; };
	virtual bool Draw(int x, int y, int currentactivetype, Font* font) = 0;

	int m_Type;
	int m_ID;
	int m_Active;
	int m_Group;
	int m_Visible;

	int m_PosX;
	int m_PosY;

};

//  The Button element can reference an internal bitmap that stores the button
//  in its various states (normal, hot, clicked, and inactive).  If you do
//  not specify a valid bitmap file, the button will still work but nothing
//  will be drawn.  You can then use panels and textlabels to actually draw
//  the button; this is good for temp work.
class GuiButton : public GuiElement
{
public:
	GuiButton(int id, int active, int group, int tilex, int tiley, int posx, int posy, int width, int height, std::string bitmap);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);
	
	int m_TileX;  //  Upper-right location of this tile quad on the tile sheet
	int m_TileY;

	int m_Width;  //  Width and height of ONE of the tiles in the quad
	int m_Height;

	Bitmap* m_ButtonBitmap;
};


//  The scrollbar does not include a background.  Use a bitmap or a panel for the
//  background.
class GuiScrollBar : public GuiElement
{
public:
	GuiScrollBar(int id, int active, int group, int posx, int posy, int width, int height, int spurlocation, int r, int g, int b, int a);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);

	int m_Width;  //  Width and height of ONE of the tiles in the quad
	int m_Height;

	int m_SpurLocation;

	int m_R, m_G, m_B, m_A;
};


//  The textarea does not include a background.  Use a bitmap or panel for the
//  background.
class GuiTextArea : public GuiElement
{
public:
	GuiTextArea(int id, int active, int group, int posx, int posy, int width, int height, int r, int g, int b, int a, std::string initialstring);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);

	int m_Width;
	int m_Height;

	int m_R;
	int m_G;
	int m_B;
	int m_A;

	std::string m_String;
};


//  Radio buttons are grouped together with a group identifier.  They have
//  an additional "set" flag that shows which of the group is currently
//  active.  Clicking any button in the group sets that button and causes all
//  other buttons in that group to come unset.
class GuiRadioButton : public GuiElement
{
public:
	GuiRadioButton(int id, int active, int group, int tilex, int tiley, int posx, int posy, int width, int height, int radiobuttongroup, int set, std::string bitmap);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);

	int m_TileX;  //  Upper-right location of this tile quad on the tile sheet
	int m_TileY;

	int m_Width;  //  Width and height of ONE of the tiles in the quad
	int m_Height;

	int m_RadioButtonGroup;
	bool m_Set;

	Bitmap* m_ButtonBitmap;
};


//  A panel is basically just a box drawn on the screen.  It can be any color,
//  it can be alpha'd and it can be empty or filled.
class GuiPanel : public GuiElement
{
public:
	GuiPanel(int id, int active, int posx, int group, int posy, int width, int height, bool filled, int r, int g, int b, int a);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);

	int m_Width;
	int m_Height;

	bool m_Filled;

	int m_R, m_G, m_B, m_A;
};


//  A bitmap is a bitmap.
class GuiBitmap : public GuiElement
{
public:
	GuiBitmap(int id, int active, int group, int posx, int posy, std::string bitmap);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);

	Bitmap* m_Bitmap;

};

//  A textlabel is just text printed at a specific location in a specific color.
class GuiTextLabel : public GuiElement
{
public:
	GuiTextLabel(int id, int active, int group, int posx, int posy, int r, int g, int b, int a, std::string initialstring);
	virtual bool Draw(int x, int y, int currentactivetype, Font* font);

	int m_R, m_G, m_B, m_A;
	std::string m_String;
};




class PlanitiaGui : public PlanitiaObject
{
public:
	PlanitiaGui() { }
	virtual ~PlanitiaGui() { Shutdown(); }
	
	virtual void Init(const std::string& configfile);
	virtual void Update();
	virtual void Shutdown();
	
	virtual void Load(std::string fileName);
	virtual void Draw();  //  This runs through each GUI element, sees if it's active, and draws it.
	virtual void RunGUI();  //  This performs actions based on what the current hot and active GUI elements are.
	virtual void ShowGroup(int group);
	virtual void HideGroup(int group);
	
	std::vector<GuiElement*> m_GuiList;
	int m_Active; //  ID of the current active GUI element
	int m_Type;  //  Type of the current active GUI element
	
	int m_GuiX;  //  These are the location of the main GUI page; elements are done as an offset to it.
	int m_GuiY;

	Font* m_Font = nullptr;
};

#endif // PLANITIA_GUI_H