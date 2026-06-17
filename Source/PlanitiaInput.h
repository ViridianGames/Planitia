#ifndef PLANITIA_INPUT_H
#define PLANITIA_INPUT_H

#include "PlanitiaObject.h"
#include <string>

class PlanitiaInput : public PlanitiaObject
{
public:
    float m_MouseX = 0;
    float m_MouseY = 0;
    float m_MouseZ = 0;
    bool m_IsLeftButtonDown = false;
    bool m_WasLeftButtonDown = false;
    bool m_IsRightButtonDown = false;
    bool m_WasRightButtonDown = false;

    void Init(const std::string& configfile) override;
    void Shutdown() override;
    void Update() override;
    void Draw() override;
};

#endif