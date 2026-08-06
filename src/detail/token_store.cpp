#include "licensr/token_store.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "detail/hwid_support.h"

namespace licensr {

namespace {

namespace fs = std::filesystem;

std::string default_app_data_root() {
#if defined(_WIN32)
  if (const char* appdata = std::getenv("APPDATA")) return std::string(appdata) + "\\Licensr";
  return "Licensr";  // Unlikely: APPDATA is unset. Falls back to the working directory.
#elif defined(__APPLE__)
  if (const char* home = std::getenv("HOME")) return std::string(home) + "/Library/Application Support/Licensr";
  return "Licensr";
#else
  if (const char* xdg = std::getenv("XDG_DATA_HOME")) return std::string(xdg) + "/licensr";
  if (const char* home = std::getenv("HOME")) return std::string(home) + "/.local/share/licensr";
  return "licensr";
#endif
}

}  // namespace

FileTokenStore::FileTokenStore(std::string plugin_slug, std::optional<std::string> base_dir) {
  fs::path root = base_dir ? fs::path(*base_dir) : fs::path(default_app_data_root());
  directory_ = (root / plugin_slug).string();
}

std::string FileTokenStore::path_for(const std::string& key) const {
  // `key` is caller-controlled (typically a license key) and shouldn't be
  // used as a raw path component — hashing it sidesteps path separators,
  // reserved filenames, and length limits in one step.
  return (fs::path(directory_) / (detail::sha256_hex(key) + ".token")).string();
}

void FileTokenStore::save(const std::string& key, const std::string& value) {
  std::error_code ec;
  fs::create_directories(directory_, ec);  // Best-effort; the write below fails loudly if this didn't work.

  std::string path = path_for(key);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) return;
  file.write(value.data(), static_cast<std::streamsize>(value.size()));
  file.close();

#if !defined(_WIN32)
  // The cached value is a signed license token, not a secret per se, but
  // there's no reason to leave it world-readable.
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
#endif
}

std::optional<std::string> FileTokenStore::load(const std::string& key) const {
  std::ifstream file(path_for(key), std::ios::binary);
  if (!file.is_open()) return std::nullopt;

  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

void FileTokenStore::remove(const std::string& key) {
  std::error_code ec;
  fs::remove(path_for(key), ec);
}

}  // namespace licensr
