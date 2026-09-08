#pragma once
#include <functional>
#include <string>
#include <vector>

namespace prelude {
using MeasureFn = std::function<int(const std::string&)>;
// Greedy word wrap. Honors '\n'. Words wider than maxWidth are split by
// character. Never returns more than maxLines lines.
std::vector<std::string> wrapText(const std::string& text, int maxWidth,
                                  const MeasureFn& measure, size_t maxLines);
}  // namespace prelude
