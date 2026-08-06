#include "yaml_lite.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace licensr_conformance {

using licensr::detail::Json;

namespace {

struct Line {
  int indent;
  std::string content;  // trimmed, blank/comment-free
  int line_no;          // 1-based, for error messages
};

std::string rtrim(const std::string& s) {
  size_t end = s.size();
  while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  return s.substr(0, end);
}

std::string trim(const std::string& s) {
  size_t begin = 0;
  while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
  return rtrim(s.substr(begin));
}

std::vector<Line> split_lines(const std::string& text) {
  std::vector<Line> lines;
  std::istringstream stream(text);
  std::string raw;
  int line_no = 0;
  while (std::getline(stream, raw)) {
    ++line_no;
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();

    size_t indent = 0;
    while (indent < raw.size() && raw[indent] == ' ') ++indent;
    if (indent < raw.size() && raw[indent] == '\t') {
      throw std::runtime_error("yaml_lite: tab indentation at line " + std::to_string(line_no));
    }

    std::string content = rtrim(raw.substr(indent));
    if (content.empty() || content[0] == '#') continue;  // blank line or whole-line comment

    lines.push_back({static_cast<int>(indent), content, line_no});
  }
  return lines;
}

bool starts_with(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Splits a flow sequence's inner text on top-level commas. No nested
// brackets appear in conformance.yaml, so this only needs to track quote
// state, not bracket depth.
std::vector<std::string> split_flow_items(const std::string& inner) {
  std::vector<std::string> items;
  std::string current;
  bool in_quotes = false;
  for (char c : inner) {
    if (c == '"') in_quotes = !in_quotes;
    if (c == ',' && !in_quotes) {
      items.push_back(trim(current));
      current.clear();
      continue;
    }
    current += c;
  }
  if (!trim(current).empty()) items.push_back(trim(current));
  return items;
}

bool looks_like_integer(const std::string& s) {
  if (s.empty()) return false;
  size_t i = (s[0] == '-') ? 1 : 0;
  if (i >= s.size()) return false;
  for (; i < s.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
  }
  return true;
}

Json parse_scalar(const std::string& raw) {
  std::string value = trim(raw);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return Json(value.substr(1, value.size() - 2));
  }
  if (value == "{}") return Json::make_object();
  if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
    auto array = Json::make_array();
    for (const auto& item : split_flow_items(value.substr(1, value.size() - 2))) {
      array.push_back(parse_scalar(item));
    }
    return array;
  }
  if (value == "true") return Json(true);
  if (value == "false") return Json(false);
  if (value == "null" || value == "~") return Json();
  if (looks_like_integer(value)) return Json(static_cast<std::int64_t>(std::stoll(value)));
  return Json(value);
}

Json parse_block(const std::vector<Line>& lines, size_t& idx, int indent);

// Parses one `key: value` mapping entry starting at `lines[idx]` (whose
// indent must equal `key_indent`), advancing `idx` past it and any
// continuation lines the value consumes (a nested block, or a folded `>`
// scalar's continuation lines).
std::pair<std::string, Json> parse_entry(const std::vector<Line>& lines, size_t& idx, int key_indent) {
  const Line& line = lines[idx];
  auto separator = line.content.find(": ");
  std::string key;
  std::string rest;
  if (separator != std::string::npos) {
    key = trim(line.content.substr(0, separator));
    rest = trim(line.content.substr(separator + 2));
  } else if (!line.content.empty() && line.content.back() == ':') {
    key = trim(line.content.substr(0, line.content.size() - 1));
    rest = "";
  } else {
    throw std::runtime_error("yaml_lite: expected 'key: value' at line " + std::to_string(line.line_no) + ": " +
                             line.content);
  }
  ++idx;

  if (!rest.empty() && rest[0] == '>') {
    // Folded block scalar: join continuation lines with spaces. None of
    // the conformance cases assert on these (they're all `description`),
    // so the exact folding rules don't need to be faithful — just needs
    // to correctly consume the continuation lines' indentation.
    std::string folded;
    while (idx < lines.size() && lines[idx].indent > key_indent) {
      if (!folded.empty()) folded += " ";
      folded += lines[idx].content;
      ++idx;
    }
    return {key, Json(folded)};
  }

  if (!rest.empty()) return {key, parse_scalar(rest)};

  if (idx < lines.size() && lines[idx].indent > key_indent) {
    return {key, parse_block(lines, idx, lines[idx].indent)};
  }
  return {key, Json()};
}

Json parse_block(const std::vector<Line>& lines, size_t& idx, int indent) {
  if (idx >= lines.size() || lines[idx].indent != indent) {
    throw std::runtime_error("yaml_lite: expected content at indent " + std::to_string(indent));
  }

  if (starts_with(lines[idx].content, "- ") || lines[idx].content == "-") {
    auto array = Json::make_array();
    while (idx < lines.size() && lines[idx].indent == indent &&
           (starts_with(lines[idx].content, "- ") || lines[idx].content == "-")) {
      const Line& line = lines[idx];
      std::string rest = line.content.size() > 1 ? trim(line.content.substr(2)) : "";
      int item_indent = indent + 2;

      if (rest.empty()) {
        ++idx;
        if (idx < lines.size() && lines[idx].indent > indent) {
          array.push_back(parse_block(lines, idx, lines[idx].indent));
        } else {
          array.push_back(Json());
        }
        continue;
      }

      // A sequence item that opens a mapping inline (`- id: foo`) has its
      // first key/value on the "- " line itself; any further keys of that
      // same item are indented to align with where "id" started.
      if (rest.find(": ") != std::string::npos || rest.back() == ':') {
        auto object = Json::make_object();
        // Splice the "- " line's remainder in as if it were its own line
        // at item_indent, so parse_entry can be reused unmodified.
        std::vector<Line> spliced = {{item_indent, rest, line.line_no}};
        size_t spliced_idx = 0;
        auto first_entry = parse_entry(spliced, spliced_idx, item_indent);
        object.set(first_entry.first, std::move(first_entry.second));
        ++idx;
        while (idx < lines.size() && lines[idx].indent == item_indent) {
          auto entry = parse_entry(lines, idx, item_indent);
          object.set(entry.first, std::move(entry.second));
        }
        array.push_back(std::move(object));
        continue;
      }

      array.push_back(parse_scalar(rest));
      ++idx;
    }
    return array;
  }

  auto object = Json::make_object();
  while (idx < lines.size() && lines[idx].indent == indent) {
    auto entry = parse_entry(lines, idx, indent);
    object.set(entry.first, std::move(entry.second));
  }
  return object;
}

}  // namespace

Json parse_yaml(const std::string& text) {
  std::vector<Line> lines = split_lines(text);
  if (lines.empty()) return Json::make_object();
  size_t idx = 0;
  return parse_block(lines, idx, lines.front().indent);
}

}  // namespace licensr_conformance
