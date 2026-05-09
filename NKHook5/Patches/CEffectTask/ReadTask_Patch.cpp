#include "ReadTask_Patch.h"

#include "../../Utils.h"
#include "../../Signatures/Signature.h"

#include <Util/Macro.h>

namespace NKHook5
{
    namespace Patches
    {
        namespace CEffectTask
        {
            using namespace Signatures;

            auto ReadTask_Patch::Apply() -> bool
            {
				const uintptr_t address = std::bit_cast<uintptr_t>(Signatures::GetAddressOf(Sigs::CEffectTask_ReadTask_Patch));
				if (address == 0)
					return false;
				// The v4.7 retail parser can assume an Effect task has SpriteFile metadata and
				// then walk uninitialised sprite state when the property is omitted. Force the
				// vulnerable JSON-property probe to return false so the original runtime takes
				// its no-sprite path instead of dereferencing missing effect-sprite data.
				static const uint32_t patchLen = 5;
				this->WriteBytes(address, "\x33\xC0\x90\x90\x90", patchLen);
				return true;
            }
        }
    }
}
