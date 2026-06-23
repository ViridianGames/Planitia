#pragma warning(disable:4786)

#ifndef _UNIT_H_
#define _UNIT_H_

#include "../Geist/Source/Object.h"
#include "../Geist/Source/Config.h"

#include <string>

class Unit : public Object
{
public:
   Unit() = default;
   virtual ~Unit();

   virtual void Init(const std::string& configfile) override;
   virtual void Shutdown();
   virtual void Update() override;
   virtual void Draw() override;

   virtual bool IsDead() { return m_IsDead; }

   bool m_IsDead;

   Config m_UnitConfig;
};

#endif