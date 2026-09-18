#ifndef _MAINSTATE_H_
#define _MAINSTATE_H_

#include "Geist/State.h"
#include "MainHud.h"
#include "raylib.h"

#include <string>

class MainState : public State
{
public:
	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

private:
	void FocusCameraOnLocalTown();

	Camera3D m_Camera{};
	float m_CameraAngle = 0.45f; // radians around Y
	float m_CameraDistance = 42.0f; // pulled back for a wider town view
	Vector3 m_LookAt{};
	MainHud m_Hud;
};

#endif
