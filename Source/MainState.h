#ifndef PLANITIA_MAIN_STATE_H
#define PLANITIA_MAIN_STATE_H

#include "Geist/State.h"
#include "Terrain.h"
#include <vector>

class MainState : public State
{
public:
    MainState() = default;
    ~MainState() override;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;

    void LoadLevelGeometry(const std::string& levelFileName);

private:
    std::vector<int>* g_UnitLocationsPlaceholder = nullptr;
};

#endif