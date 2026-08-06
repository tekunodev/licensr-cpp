// Linux: `/etc/machine-id`, a systemd-maintained 32-hex-character
// identifier stable across reboots and regenerated only on OS reinstall
// (or explicit `systemd-machine-id-setup --commit`). Falls back to
// `/var/lib/dbus/machine-id` (the pre-systemd/non-systemd equivalent,
// often bind-mounted to the same file anyway) for minimal/container
// images that lack the former.
#include <fstream>
#include <optional>
#include <string>

#include "detail/hwid_support.h"
#include "licensr/hwid.h"

namespace {

std::optional<std::string> read_machine_id_file(const char* path) {
  std::ifstream file(path);
  if (!file.is_open()) return std::nullopt;

  std::string contents;
  std::getline(file, contents);
  while (!contents.empty() && (contents.back() == '\n' || contents.back() == '\r' || contents.back() == ' ')) {
    contents.pop_back();
  }
  if (contents.empty()) return std::nullopt;
  return contents;
}

}  // namespace

namespace licensr {

std::optional<std::string> compute_hardware_id(const std::string& salt) {
  auto raw_id = read_machine_id_file("/etc/machine-id");
  if (!raw_id) raw_id = read_machine_id_file("/var/lib/dbus/machine-id");
  if (!raw_id) return std::nullopt;

  return detail::hash_hardware_id(salt, *raw_id);
}

}  // namespace licensr
