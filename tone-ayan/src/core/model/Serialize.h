#pragma once

#include "Project.h"

#include <string>

namespace tone
{
/** JSON round-trip of the full edit model (used for plugin state, ARA archives and the CLI). */
std::string projectToJson (const Project&, int indent = -1);
Project projectFromJson (const std::string&); // throws std::exception on malformed input
} // namespace tone
