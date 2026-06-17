#include "PlanitiaGui.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaDisplay.h"
#include "PlanitiaInput.h"
#include "PlanitiaResourceManager.h"
#include "PlanitiaFonts.h"
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

void PlanitiaGui::Init(const std::string& configfile)
{
	m_Font = &g_PlanitiaFont;
	Load(configfile);
	m_Active = 0;
}

void PlanitiaGui::Update()
{
	RunGUI();
}

void PlanitiaGui::Draw()
{
	vector<GuiElement*>::iterator node = m_GuiList.begin();

	//  Okay, what we do here will be based on what is active and what type
	//  it is.  This will allow us to do persistent behavior on some objects
	//  like the textarea (which, once activated, will not become unactivated
	//  until the user presses ENTER.

	bool _GotAHitThisFrame = false;


	for(node; node != m_GuiList.end(); ++node)
	{
		int _ReturnState = (*node)->Draw(m_GuiX, m_GuiY, m_Type, m_Font);

		if(_ReturnState == 1)  //  Hot normal button or radio button
		{
			_GotAHitThisFrame = true;
			m_Active = (*node)->m_ID;
			m_Type = (*node)->m_Type;

			if(m_Type == GUI_RADIOBUTTON)
			{
				m_Type = (*node)->m_Type;
				GuiRadioButton* _NewRadioSet = dynamic_cast<GuiRadioButton*>(*node);
				m_Active = _NewRadioSet->m_ID;
				int _SetGroup = _NewRadioSet->m_Group;

				//  Unset all others in the group
				vector<GuiElement*>::iterator node2 = m_GuiList.begin();
				for(node2; node2 != m_GuiList.end(); ++node2)
				{
					if((*node2)->m_Type == GUI_RADIOBUTTON)
					{
						GuiRadioButton* temp = dynamic_cast<GuiRadioButton*>(*node2);
						if(temp->m_Group == _SetGroup)
						{
							if(temp->m_ID != m_Active)
							{
								temp->m_Set = false;
							}
						}
					}
				}
			}
		}
	}

	if(!_GotAHitThisFrame)
	{
		m_Active = 0;
	}
}

void PlanitiaGui::RunGUI()
{
    
}

void PlanitiaGui::Shutdown()
{
	
}

//  A file that defines a gui starts with the filename for the buttons for
//  this GUI.  Then each line after that defines a single button.  Example:
//
//  images\\combat.tga
//  ID Active TileX TileY PosX PosY Width Height;

void PlanitiaGui::Load(std::string fileName)
{
	ifstream instream(fileName.c_str());
	if(!instream.fail())  //  Open successful!
	{

		string line;
		

		stringstream parser;
		instream >> m_GuiX;
		instream >> m_GuiY;

		getline(instream, line);
		getline(instream, line); // legacy font config line (ignored; uses Raylib TTF fonts)

		while(!instream.eof())
		{
			getline(instream, line);

			if(line[0] != '#') //  If this line is not a comment
			{
				stringstream _LineData;
				_LineData << line;
				int type;
				_LineData >> type;
				switch(type)
				{
				case GUI_BUTTON:
					{
						int buttonid, active, group, tilex, tiley, posx, posy, width, height;
						string bitmap;
						_LineData >> buttonid;
						_LineData >> active;
						_LineData >> group;
						_LineData >> tilex;
						_LineData >> tiley;
						_LineData >> posx;
						_LineData >> posy;
						_LineData >> width;
						_LineData >> height;

						char _Buffer[256];
						_LineData.get(_Buffer, 255, '\r');
						bitmap = _Buffer;
						bitmap.erase(0, 1);  //  Kill the leading space.

						GuiButton *temp = new GuiButton(buttonid, active, group, tilex, tiley, posx, posy, width, height, bitmap);
						m_GuiList.push_back(temp);
					}
					break;

				case GUI_SCROLLBAR:
					{
						int buttonid, active, group, posx, posy, width, height, spurlocation, r, g, b, a;
						string bitmap;
						_LineData >> buttonid;
						_LineData >> active;
						_LineData >> group;
						_LineData >> posx;
						_LineData >> posy;
						_LineData >> width;
						_LineData >> height;
						_LineData >> spurlocation;
						_LineData >> r;
						_LineData >> g;
						_LineData >> b;
						_LineData >> a;

						GuiScrollBar *temp = new GuiScrollBar(buttonid, active, group, posx, posy, width, height, spurlocation, r, g, b, a);
						m_GuiList.push_back(temp);
					}
					break;


				case GUI_TEXTAREA:
					{
						int id, active, x, y, group, width, height, r, g, b, a;
						string initialtext;
						_LineData >> id;
						_LineData >> active;
						_LineData >> group;
						_LineData >> x;
						_LineData >> y;
						_LineData >> width;
						_LineData >> height;
						_LineData >> r;
						_LineData >> g;
						_LineData >> b;
						_LineData >> a;

						char _Buffer[256];
						_LineData.get(_Buffer, 255, '\n');
						initialtext = _Buffer;
						initialtext.erase(0, 1);  //  Kill the leading space.

						GuiTextArea *temp = new GuiTextArea(id, active, group, x, y, width, height, r, g, b, a, initialtext);
						m_GuiList.push_back(temp);
					}
					break;



				case GUI_RADIOBUTTON:
					{
						int buttonid, active, group, tilex, tiley, posx, posy, width, height, radiobuttongroup, set;
						string bitmap;
						_LineData >> buttonid;
						_LineData >> active;
						_LineData >> group;
						_LineData >> tilex;
						_LineData >> tiley;
						_LineData >> posx;
						_LineData >> posy;
						_LineData >> width;
						_LineData >> height;
						_LineData >> radiobuttongroup;
						_LineData >> set;

						char _Buffer[256];
						_LineData.get(_Buffer, 255, '\n');
						bitmap = _Buffer;
						bitmap.erase(0, 1);  //  Kill the leading space.

						GuiRadioButton *temp = new GuiRadioButton(buttonid, active, group, tilex, tiley, posx, posy, width, height, radiobuttongroup, set, bitmap);
						m_GuiList.push_back(temp);
					}
					break;

				case GUI_BITMAP:
					{
						int id, active, group, tiley, posx, posy;
						string bitmap;
						_LineData >> id;
						_LineData >> active;
						_LineData >> group;
						_LineData >> posx;
						_LineData >> posy;

						char _Buffer[256];
						_LineData.get(_Buffer, 255, '\n');
						bitmap = _Buffer;
						bitmap.erase(0, 1);  //  Kill the leading space.

						GuiBitmap *temp = new GuiBitmap(id, active, group, posx, posy, bitmap);
						m_GuiList.push_back(temp);
					}
					break;

				case GUI_PANEL:
					{
						int id, active, group, x, y, width, height, r, g, b, a;
						bool filled;
						_LineData >> id;
						_LineData >> active;
						_LineData >> group;
						_LineData >> x;
						_LineData >> y;
						_LineData >> width;
						_LineData >> height;
						_LineData >> filled;
						_LineData >> r;
						_LineData >> g;
						_LineData >> b;
						_LineData >> a;
						GuiPanel *temp = new GuiPanel(id, active, group, x, y, width, height, filled, r, g, b, a);
						m_GuiList.push_back(temp);
					}
					break;

				case GUI_TEXTLABEL:
					{
						int id, active, group, x, y, r, g, b, a;
						string initialtext;
						_LineData >> id;
						_LineData >> active;
						_LineData >> group;
						_LineData >> x;
						_LineData >> y;
						_LineData >> r;
						_LineData >> g;
						_LineData >> b;
						_LineData >> a;

						char _Buffer[256];
						_LineData.get(_Buffer, 255, '\n');
						initialtext = _Buffer;
						initialtext.erase(0, 1);  //  Kill the leading space.

						GuiTextLabel *temp = new GuiTextLabel(id, active, group, x, y, r, g, b, a, initialtext);
						m_GuiList.push_back(temp);
					}
					break;
				}
			}
		}
	}
}

void PlanitiaGui::HideGroup(int group)
{
	vector<GuiElement*>::iterator node = m_GuiList.begin();
	{
		for(node; node != m_GuiList.end(); ++node)
		{
			if((*node)->m_Group == group)
			{
				(*node)->m_Visible = false;
			}
		}
	}
}

void PlanitiaGui::ShowGroup(int group)
{
	vector<GuiElement*>::iterator node = m_GuiList.begin();
	{
		for(node; node != m_GuiList.end(); ++node)
		{
			if((*node)->m_Group == group)
			{
				(*node)->m_Visible = true;
			}
		}
	}
}

//  GUIBUTTON

GuiButton::GuiButton(int buttonid, int active, int group, int tilex, int tiley, int posx, int posy,
					 int width, int height, string bitmap)
{
	m_Type = GUI_BUTTON;
	m_ID = buttonid;
	m_Active = active;
	m_Group = group;
	m_TileX = tilex;
	m_TileY = tiley;
	m_PosX = posx;
	m_PosY = posy;
	m_Width = width;
	m_Height = height;

	//  See if the actual file exists
	ifstream instream(bitmap.c_str());
	if(!instream.fail())
		m_ButtonBitmap = gp_ResourceManager->GetBitmap(bitmap);
	else
		m_ButtonBitmap = NULL;
}

bool GuiButton::Draw(int x, int y, int currentactivetype, Font* font)
{
	if(m_Visible == false)
		return false;

	bool _ReturnState = false;
	//  First off, if it's inactive, draw it inactive and that's it.
	if(m_Active == 0)
	{
		if(m_ButtonBitmap)
		{
			gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY + m_Height,
				m_Width, m_Height, x + m_PosX, y + m_PosY);
		}
	}

	//  If the mouse is over the button...
	else if(gp_Input->m_MouseX >= x + m_PosX && 
		gp_Input->m_MouseX <= x + m_PosX + m_Width &&
		gp_Input->m_MouseY >= y + m_PosY &&
		gp_Input->m_MouseY <= y + m_PosY + m_Height)
	{
		//  If the left button is down, we are CLICKED
		if(gp_Input->m_IsLeftButtonDown)
		{
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX, m_TileY + m_Height,
					m_Width, m_Height, x + m_PosX, y + m_PosY);
			}
		}

		//  If the left button has just been released, we are ACTIVE
		else if((!gp_Input->m_IsLeftButtonDown && gp_Input->m_WasLeftButtonDown) ||
			(!gp_Input->m_IsRightButtonDown && gp_Input->m_WasRightButtonDown))
		{
			_ReturnState = true;  //  Active radio button
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY,
					m_Width, m_Height, x + m_PosX, y + m_PosY);
			}
		}

		else  //  Left button is not down and we didn't just finish a click, we are HOT, but not yet active.
		{
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY,
					m_Width, m_Height, x + m_PosX, y + m_PosY);
			}
		}
	}
	else  //  Not hot, clicked, active or inactive, so just draw the button normally.
	{
		if(m_ButtonBitmap)
		{
			gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX, m_TileY, m_Width,
				m_Height, x + m_PosX, y + m_PosY);
		}
	}

	return _ReturnState;
};

//  GUISCROLLBAR
GuiScrollBar::GuiScrollBar(int id, int active, int group, int posx, int posy, int width, int height, int spurlocation, int r, int g, int b, int a)
{
	m_Type = GUI_SCROLLBAR;
	m_ID = id;
	m_Active = active;
	m_Group = group;
	m_PosX = posx;
	m_PosY = posy;
	m_Width = width;
	m_Height = height;
	m_SpurLocation = spurlocation;
	m_R = r;
	m_B = b;
	m_G = g;
	m_A = a;
}

bool GuiScrollBar::Draw(int x, int y, int currentactivetype, Font* font)
{
	if(m_Visible == false)
		return false;

	//  All we really do is draw the spur.  If the LMB is down inside the area, we change the spur's location vertically.
	//  The spur will be eight pixels high and the width of the scrollbox minus two.

	bool _ReturnValue = false;

	if(gp_Input->m_MouseX >= x + m_PosX && 
		gp_Input->m_MouseX <= x + m_PosX + m_Width &&
		gp_Input->m_MouseY >= y + m_PosY &&
		gp_Input->m_MouseY <= y + m_PosY + m_Height)
	{
		//  If the left button is down, we are CLICKED
		if(gp_Input->m_IsLeftButtonDown)
		{
			m_SpurLocation = gp_Input->m_MouseY - y - m_PosY;
			_ReturnValue = true;
		}
	}

	gp_Display->DrawBox(x + m_PosX + 1, y + m_PosY + m_SpurLocation - 4, m_Width - 2, 8, m_R, m_G, m_B, m_A, true);

	return _ReturnValue;
}

//  GUITEXTAREA

GuiTextArea::GuiTextArea(int id, int active, int group, int posx, int posy,
						 int width, int height, int r, int g, int b, int a, std::string initialstring)
{
	m_Type = GUI_TEXTAREA;
	m_ID = id;
	m_Active = active;
	m_Group = group;
	m_PosX = posx;
	m_PosY = posy;
	m_R = r;
	m_G = g;
	m_B = b;
	m_A = a;
	m_String = initialstring;

}

bool GuiTextArea::Draw(int x, int y, int currentactivetype, Font* font)
{
	if(m_Visible == false)
		return false;

	if (font && font->texture.id != 0)
		PlanitiaDrawText(*font, PLANITIA_FONT_SIZE, m_String,
			static_cast<float>(x + m_PosX), static_cast<float>(y + m_PosY), m_R, m_G, m_B, m_A);

	if(gp_Input->m_MouseX >= x + m_PosX && 
		gp_Input->m_MouseX <= x + m_PosX + m_Width &&
		gp_Input->m_MouseY >= y + m_PosY &&
		gp_Input->m_MouseY <= y + m_PosY + m_Height)
	{
		//  If the left button is down, we are CLICKED
		if(gp_Input->m_IsLeftButtonDown)
		{

		}


	}

	return false;
}

//  GUIRADIOBUTTON

GuiRadioButton::GuiRadioButton(int buttonid, int active, int group, int tilex, int tiley, int posx, int posy,
					 int width, int height, int radiobuttongroup, int set, string bitmap)
{
	m_Type = GUI_RADIOBUTTON;
	m_ID = buttonid;
	m_Active = active;
	m_Group = group;
	m_TileX = tilex;
	m_TileY = tiley;
	m_PosX = posx;
	m_PosY = posy;
	m_Width = width;
	m_Height = height;
	m_RadioButtonGroup = radiobuttongroup;
	m_Set = set;

	//  See if the actual file exists
	ifstream instream(bitmap.c_str());
	if(!instream.fail())
		m_ButtonBitmap = gp_ResourceManager->GetBitmap(bitmap);
	else
		m_ButtonBitmap = NULL;
}

bool GuiRadioButton::Draw(int x, int y, int currentactivetype, Font* font)
{

	if(m_Visible == false)
		return false;

	bool _ReturnState = false;
	//  First off, if it's inactive, draw it inactive and that's it.
	if(m_Active == 0)
	{
		if(m_ButtonBitmap)
		{
			gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY + m_Height,
				m_Width, m_Height, x + m_PosX, y + m_PosY);
		}
	}

	//  If the mouse is over the button...
	else if(gp_Input->m_MouseX >= x + m_PosX && 
		gp_Input->m_MouseX <= x + m_PosX + m_Width &&
		gp_Input->m_MouseY >= y + m_PosY &&
		gp_Input->m_MouseY <= y + m_PosY + m_Height)
	{
		//  If the left button is down, we are CLICKED
		if(gp_Input->m_IsLeftButtonDown)
		{
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX, m_TileY + m_Height,
					m_Width, m_Height, x + m_PosX, y + m_PosY);
			}
			m_Set = true;
			_ReturnState = true;  //  Active radio button
		}

		//  If the left button has just been released, we are ACTIVE
		else if(!gp_Input->m_IsLeftButtonDown && gp_Input->m_WasLeftButtonDown)
		{
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY,
					m_Width, m_Height, x + m_PosX, y + m_PosY);
			}
		}

		else  //  No "hot" state for radio buttons; looks bad.
		{
			if(m_Set)
			{
				if(m_ButtonBitmap)
				{
					gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX, m_TileY,
						m_Width, m_Height, x + m_PosX, y + m_PosY);
				}
			}
			else
			{
				if(m_ButtonBitmap)
				{
					gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY + m_Height, m_Width,
						m_Height, x + m_PosX, y + m_PosY);
				}
			}
		}
	}
	else  //  Not hot, clicked, active or inactive, so just draw the button normally.
	{
		if(m_Set)
		{
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX, m_TileY,
					m_Width, m_Height, x + m_PosX, y + m_PosY);
			}
		}
		else
		{
			if(m_ButtonBitmap)
			{
				gp_Display->BlitImageRect(m_ButtonBitmap, m_TileX + m_Width, m_TileY + m_Height, m_Width,
					m_Height, x + m_PosX, y + m_PosY);
			}
		}
	}

	return _ReturnState;
};

//  GUIPANEL
GuiPanel::GuiPanel(int id, int active, int group, int posx, int posy, int width,
				   int height, bool filled, int r, int g, int b, int a)
{
	m_Type = GUI_PANEL;
	m_ID = id;
	m_Active = active;
	m_Group = group;
	m_PosX = posx;
	m_PosY = posy;
	m_Width = width;
	m_Height = height;
	m_Filled = filled;
	m_R = r;
	m_G = g;
	m_B = b;
	m_A = a;
}

bool GuiPanel::Draw(int x, int y, int currentactivetype, Font* font)
{
	if(m_Visible == false)
		return false;

	gp_Display->DrawBox(x + m_PosX, y + m_PosY, m_Width, m_Height, m_R, m_G, m_B, m_A, m_Filled);
	return false;
};

//  GUIBITMAP
GuiBitmap::GuiBitmap(int id, int active, int group, int posx, int posy, string bitmap)
{
	m_Type = GUI_BITMAP;
	m_ID = id;
	m_Active = active;
	m_Group = group;
	m_PosX = posx;
	m_PosY = posy;

	//  See if the actual file exists
	ifstream instream(bitmap.c_str());
	if(!instream.fail())
		m_Bitmap = gp_ResourceManager->GetBitmap(bitmap);
	else
		m_Bitmap = NULL;
}

bool GuiBitmap::Draw(int x, int y, int currentactivetype, Font* font)
{
	if(m_Visible == false)
		return false;

	if(m_Bitmap)
	{
		gp_Display->BlitImage(m_Bitmap, x + m_PosX, y + m_PosY);
	}
	return false;
};


//  GUITEXTLABEL
GuiTextLabel::GuiTextLabel(int id, int active, int group, int posx, int posy,
						 int r, int g, int b, int a, std::string initialstring)
{
	m_Type = GUI_TEXTLABEL;
	m_ID = id;
	m_Active = active;
	m_Group = group;
	m_PosX = posx;
	m_PosY = posy;
	m_R = r;
	m_G = g;
	m_B = b;
	m_A = a;
	m_String = initialstring;

}

bool GuiTextLabel::Draw(int x, int y, int currentactivetype, Font* font)
{
	if(m_Visible == false)
		return false;

	if (font && font->texture.id != 0)
		PlanitiaDrawText(*font, PLANITIA_FONT_SIZE, m_String,
			static_cast<float>(x + m_PosX), static_cast<float>(y + m_PosY), m_R, m_G, m_B, m_A);
	return false;
};