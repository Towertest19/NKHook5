#include "Glob.h"

using namespace Common;
using namespace Common::Util;
namespace fs = std::filesystem;

std::string Sanitize(std::string filePath) {
	while (filePath.find("\\") != std::string::npos) {
		size_t wrongSlashPos = filePath.find("\\");
		filePath[wrongSlashPos] = '/';
	}
	return filePath;
}

bool MatchPart(std::string pathPart, std::string globPart) {
	//If the glob part is just a wildcard, it will always patch
	if (globPart == "*") {
		return true;
	}

	//If there is a file extension, we want to make sure they match
	if (globPart.find(".") != std::string::npos && pathPart.find(".") != std::string::npos) {
		//Get the glob's stub
		std::string globStub = globPart.substr(0, globPart.find("."));
		//Get the glob's extension
		std::string globExt = globPart.substr(globPart.find(".")+1);

		//Get the path's stub
		std::string pathStub = pathPart.substr(0, pathPart.find("."));
		//Get the path's extension
		std::string pathExt = pathPart.substr(pathPart.find(".")+1);

		bool stubsMatch = MatchPart(pathStub, globStub);
		bool extensionsMatch = MatchPart(pathExt, globExt);

		return stubsMatch && extensionsMatch;
	}

	//In any other case, we just want to know if the strings match
	return pathPart == globPart;
}

Glob::Glob(std::string globStr) {
	//First we need to sanitize the path and replace all \\ with /
	std::string sanitizedGlob = Sanitize(globStr);

	//Split the globs parts
	while (sanitizedGlob.find("/") != std::string::npos) {
		size_t slashPos = sanitizedGlob.find("/");
		//Get the individual part
		std::string part = sanitizedGlob.substr(0, slashPos);
		//Append it to the vector
		this->globParts.push_back(part);
		//Next part
		sanitizedGlob = sanitizedGlob.substr(slashPos+1);
	}

	//Push the last part of the glob (stored in sanatizedGlob)
	if(!sanitizedGlob.empty())
		this->globParts.push_back(sanitizedGlob);
}

bool Glob::Match(std::string filePath) const {
	//First we need to sanitize the path and replace all \\ with /
	std::string sanitizedPath = Sanitize(filePath);

	std::vector<std::string> pathParts;
	//Split the globs parts
	while (sanitizedPath.find("/") != std::string::npos) {
		size_t slashPos = sanitizedPath.find("/");
		//Get the individual part
		std::string part = sanitizedPath.substr(0, slashPos);
		//Append it to the vector
		pathParts.push_back(part);
		//Next part
		sanitizedPath = sanitizedPath.substr(slashPos+1);
	}

	//Push the last part of the path (stored in sanatizedPath)
	if (!sanitizedPath.empty())
		pathParts.push_back(sanitizedPath);

	// Compare glob vs path using a two-pointer approach.
	// A leading '*' in the glob can match any remaining leading path parts.
	// Each subsequent glob part is matched character-by-character against
	// the corresponding path part (a trailing '*' glob part can absorb the
	// rest of the path part as a suffix match).
	size_t g = 0;  // glob index
	size_t p = 0;  // path index
	std::string::size_type lastStarPatternPos = std::string::npos;

	while (p < pathParts.size()) {
		if (g < globParts.size() &&
			(globParts[g] == "?" || globParts[g] == pathParts[p])) {
			++g;
			++p;
		}
		else if (g < globParts.size() && globParts[g] == "*") {
			lastStarPatternPos = g++;
		}
		else if (lastStarPatternPos != std::string::npos) {
			// Backtrack: try advancing the path offset by one
			// (the '*' absorbs one more path part). The glob pointer
			// resets one position past the saved star so that the
			// next fixed/trailing-wildcard glob part tries again on
			// the next path part.
			g = lastStarPatternPos + 1;
			++p;
		}
		else {
			return false;
		}
	}

	// Consume any trailing glob wildcards
	while (g < globParts.size() && globParts[g] == "*") {
		++g;
	}

	return g == globParts.size();
}
