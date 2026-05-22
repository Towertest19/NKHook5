#pragma once

#include "Glob.h"

#include <string>

namespace Common::Util
{
	// Extension targets use path-style globs (e.g. "*/Assets/JSON/LabDefinitions/*.json").
	// The part-wise Glob matcher accepts an optional leading directory segment.
	inline bool MatchesExtensionTarget(const std::string& pattern, const std::string& path)
	{
		return Glob(pattern).Match(path);
	}
}
