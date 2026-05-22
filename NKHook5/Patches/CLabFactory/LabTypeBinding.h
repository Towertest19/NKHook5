#pragma once

namespace NKHook5::Extensions::LabDefinitions { class LabDefinitionsExt; }
namespace NKHook5::Extensions::SpecialtyDefinitions { class SpecialtyDefinitionsExt; }

namespace NKHook5::Patches::CLabFactory
{
	using VanillaGetMaxLevelFn = int(__thiscall*)(void*, int);

	void SetVanillaGetMaxLevel(VanillaGetMaxLevelFn fn);
	void TryBindLabTypes(void* labFactory,
		NKHook5::Extensions::LabDefinitions::LabDefinitionsExt* labExt,
		NKHook5::Extensions::SpecialtyDefinitions::SpecialtyDefinitionsExt* specExt);
}
