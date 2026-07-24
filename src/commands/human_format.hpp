#pragma once

#include <string>

namespace commands {

// Renders JSON text for human reading: objects are printed as an indented
// tree that preserves the JSON's own key/value structure; arrays are
// printed `ls`-style - a column-aligned table when the elements are
// objects, a terminal-width-aware multi-column grid when they're scalars.
// If text doesn't parse as JSON, it's returned unchanged.
std::string formatHumanReadable(const std::string& text);

}  // namespace commands
