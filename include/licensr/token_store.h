// Platform-appropriate persistence for offline-verification tokens (see
// Client::token), so a plugin can re-verify the last known-good token on
// launch and boot without a network call, falling back to an online
// validate() only once that fails or has expired.
#pragma once

#include <optional>
#include <string>

#include "licensr/export.h"

namespace licensr {

// Bring your own implementation for hosts with their own storage (a DAW's
// plugin-state chunk, a keychain wrapper) — Licensr never requires
// `FileTokenStore` specifically, only something implementing this
// interface.
class LICENSR_API TokenStore {
 public:
  virtual ~TokenStore() = default;

  // `key` is caller-defined — typically the license key, so multiple
  // licenses can be cached side by side without colliding.
  virtual void save(const std::string& key, const std::string& value) = 0;
  virtual std::optional<std::string> load(const std::string& key) const = 0;
  virtual void remove(const std::string& key) = 0;
};

// Default file-backed TokenStore: one file per key, under a per-plugin
// directory in the platform's app-data location:
//   macOS:   ~/Library/Application Support/Licensr/<plugin_slug>/
//   Windows: %APPDATA%\Licensr\<plugin_slug>\
//   Linux:   $XDG_DATA_HOME/licensr/<plugin_slug>/ (falls back to
//            ~/.local/share/licensr/<plugin_slug>/)
//
// AUv3 app extensions run in their own sandboxed container, entirely
// separate from the host app's — the OS-default resolution above resolves
// to the *extension* process's own container, which is rarely what you
// want if the host and extension need to see the same cached token. Pass
// an explicit `base_dir` in that case: the extension's app-group container
// path if you share one with the host
// (`NSFileManager.containerURL(forSecurityApplicationGroupIdentifier:)`),
// or the extension's own Application Support directory otherwise.
class LICENSR_API FileTokenStore : public TokenStore {
 public:
  explicit FileTokenStore(std::string plugin_slug, std::optional<std::string> base_dir = std::nullopt);

  void save(const std::string& key, const std::string& value) override;
  std::optional<std::string> load(const std::string& key) const override;
  void remove(const std::string& key) override;

  // The resolved directory tokens are written under (created lazily by
  // the first save()). Exposed mainly for diagnostics and tests.
  const std::string& directory() const { return directory_; }

 private:
  std::string path_for(const std::string& key) const;

  std::string directory_;
};

}  // namespace licensr
