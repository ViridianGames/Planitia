#ifndef PLANITIA_ENGINE_ADAPTER_H
#define PLANITIA_ENGINE_ADAPTER_H

#include "PlanitiaObject.h"
#include "PlanitiaTypes.h"
#include "PlanitiaConfig.h"
#include <map>
#include <string>

class PlanitiaEngineAdapter : public PlanitiaObject
{
public:
    bool m_Done = false;
    std::map<std::string, PlanitiaConfigInfo> m_EngineConfig;
    DWORD m_GameTimeInMS = 0;
    float m_GameTimeInSeconds = 0;
    DWORD m_DurationOfLastUpdateInMS = 0;
    float m_DurationOfLastUpdateInSeconds = 0;
    UINT m_NumberOfUpdatesToDoThisFrame = 1;
    UINT m_GameUpdates = 0;
    UINT m_GameFrames = 0;
    UINT m_NumberOfMillisecondsBetweenUpdates = 33;
    DWORD m_UpdateTimer = 0;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;
    void SyncTiming();
};

#endif