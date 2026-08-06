// macOS/iOS: IOPlatformUUID, stable across reboots and OS reinstalls (it's
// tied to the logic board, not the boot volume) but *not* preserved by a
// clean OS reinstall via Apple's "Erase All Content and Settings", nor
// portable across virtualized macOS instances that don't pass through a
// stable UUID.
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include "detail/hwid_support.h"
#include "licensr/hwid.h"

namespace licensr {

std::optional<std::string> compute_hardware_id(const std::string& salt) {
  // kIOMainPortDefault: the mach port constant itself hasn't changed, only
  // its name (Apple renamed kIOMasterPortDefault in the macOS 12 SDK), so
  // this works fine at runtime even when deploying to older macOS — it
  // just requires building with an Xcode 13+ SDK.
  io_service_t platform_expert =
      IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
  if (platform_expert == 0) return std::nullopt;

  CFTypeRef uuid_property =
      IORegistryEntryCreateCFProperty(platform_expert, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
  IOObjectRelease(platform_expert);
  if (uuid_property == nullptr) return std::nullopt;

  char buffer[64] = {};
  bool ok = CFStringGetCString(static_cast<CFStringRef>(uuid_property), buffer, sizeof(buffer), kCFStringEncodingUTF8);
  CFRelease(uuid_property);
  if (!ok) return std::nullopt;

  return detail::hash_hardware_id(salt, buffer);
}

}  // namespace licensr
