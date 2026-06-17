#ifndef PLANITIA_MAIN_MENU_STATE_H
#define PLANITIA_MAIN_MENU_STATE_H

#include "Geist/State.h"
#include "Geist/Gui.h"


class MainMenuState : public State
{
public:
    MainMenuState() = default;
    ~MainMenuState() override;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;

    Gui m_FrontEnd;
    Texture* m_Cursor = nullptr;

private:
    void DoInput();
};

#endif