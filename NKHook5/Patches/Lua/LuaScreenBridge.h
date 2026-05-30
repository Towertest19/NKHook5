#pragma once

#include "../IPatch.h"
#include "../../Classes/CBloonsBaseScreen.h"

namespace NKHook5::Patches::Lua
{
	class LuaScreenBridge : public IPatch
	{
	public:
		LuaScreenBridge() : IPatch("LuaScreenBridge") {}
		bool Apply() override;
	};

	void NotifyScreenInit(Classes::CBloonsBaseScreen* screen);
	void NotifyScreenUpdate(Classes::CBloonsBaseScreen* screen);
	void NotifyScreenCleanup(Classes::CBloonsBaseScreen* screen);
}
