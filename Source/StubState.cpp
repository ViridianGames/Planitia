#include "../Geist/Source/State.h"
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaGlobals.h"

class StubState : public State
{
public:
    void Init(const std::string&) override {}
    void Shutdown() override {}
    void Update() override
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            g_Engine->m_Done = true;
            if (gp_Engine) gp_Engine->m_Done = true;
        }
    }
    void Draw() override {}
    void OnEnter() override {}
    void OnExit() override {}
};

State* CreateStubState()
{
    return new StubState();
}