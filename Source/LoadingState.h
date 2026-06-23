#ifndef PLANITIA_LOADING_STATE_H
#define PLANITIA_LOADING_STATE_H

#include "../Geist/Source/State.h"

class LoadingState : public State
{
public:
    LoadingState() = default;
    ~LoadingState() override;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;
    void OnEnter() override {}
    void OnExit() override {}

private:
    int m_Loaded = 2;
};

#endif