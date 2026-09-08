#include "prelude/word_wrap.h"

namespace prelude {
namespace {

// Splits a word that does not fit on any line into pieces that do.
void pushLongWord(const std::string& word, int maxWidth, const MeasureFn& measure,
                  size_t maxLines, std::vector<std::string>& lines, std::string& line) {
  std::string piece;
  for (char ch : word) {
    if (!piece.empty() && measure(piece + ch) > maxWidth) {
      if (lines.size() >= maxLines) return;
      lines.push_back(piece);
      piece.clear();
    }
    piece += ch;
  }
  line = piece;
}

}  // namespace

std::vector<std::string> wrapText(const std::string& text, int maxWidth,
                                  const MeasureFn& measure, size_t maxLines) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < text.size() && lines.size() < maxLines) {
    const char c = text[i];
    if (c == '\n') {
      lines.push_back(line);
      line.clear();
      ++i;
      continue;
    }
    if (c == ' ') {
      ++i;
      continue;
    }
    size_t j = i;
    while (j < text.size() && text[j] != ' ' && text[j] != '\n') ++j;
    const std::string word = text.substr(i, j - i);
    i = j;

    const std::string candidate = line.empty() ? word : line + " " + word;
    if (measure(candidate) <= maxWidth) {
      line = candidate;
      continue;
    }
    if (!line.empty()) {
      lines.push_back(line);
      line.clear();
      if (lines.size() >= maxLines) break;
    }
    if (measure(word) <= maxWidth) {
      line = word;
    } else {
      pushLongWord(word, maxWidth, measure, maxLines, lines, line);
    }
  }
  if (!line.empty() && lines.size() < maxLines) lines.push_back(line);
  return lines;
}

}  // namespace prelude
