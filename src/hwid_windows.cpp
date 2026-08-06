// Windows: `MachineGuid` (a per-install GUID Windows Setup writes to the
// registry, stable across reboots but regenerated on a clean OS install)
// combined with the system volume's serial number (stable unless the
// drive is reformatted). Neither alone is a great identifier — MachineGuid
// is trivially spoofable by anyone with an admin PowerShell prompt, and
// volume serials aren't unique across cloned drive images — but the pair
// is what LicenseSeat-class SDKs converge on for a dependency-free
// Windows fingerprint, and it's the raw input to a one-way hash regardless.
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "detail/hwid_support.h"
#include "licensr/hwid.h"

namespace {

std::string to_utf8(const wchar_t* wide, std::size_t wide_len) {
  if (wide_len == 0) return {};
  int required = WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(wide_len), nullptr, 0, nullptr, nullptr);
  if (required <= 0) return {};
  std::string utf8(static_cast<std::size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(wide_len), utf8.data(), required, nullptr, nullptr);
  return utf8;
}

std::optional<std::wstring> read_machine_guid() {
  wchar_t buffer[64] = {};
  DWORD size = sizeof(buffer);
  LONG result = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid", RRF_RT_REG_SZ,
                             nullptr, buffer, &size);
  if (result != ERROR_SUCCESS) return std::nullopt;
  return std::wstring(buffer);
}

std::optional<std::wstring> read_system_drive_root() {
  wchar_t drive[8] = {};
  DWORD len = GetEnvironmentVariableW(L"SystemDrive", drive, static_cast<DWORD>(sizeof(drive) / sizeof(drive[0])));
  if (len == 0 || len >= sizeof(drive) / sizeof(drive[0])) return std::nullopt;
  std::wstring root(drive);
  root += L"\\";
  return root;
}

std::optional<DWORD> read_volume_serial(const std::wstring& root) {
  DWORD serial = 0;
  BOOL ok = GetVolumeInformationW(root.c_str(), nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
  if (!ok) return std::nullopt;
  return serial;
}

}  // namespace

namespace licensr {

std::optional<std::string> compute_hardware_id(const std::string& salt) {
  auto machine_guid = read_machine_guid();
  if (!machine_guid) return std::nullopt;

  std::string raw_id = to_utf8(machine_guid->data(), machine_guid->size());

  if (auto root = read_system_drive_root()) {
    if (auto serial = read_volume_serial(*root)) {
      char serial_hex[16];
      std::snprintf(serial_hex, sizeof(serial_hex), "-%08lx", static_cast<unsigned long>(*serial));
      raw_id += serial_hex;
    }
  }

  return detail::hash_hardware_id(salt, raw_id);
}

}  // namespace licensr
