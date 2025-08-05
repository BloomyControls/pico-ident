#ifndef PICO_IDENT_SRC_DEVICEINFO_H
#define PICO_IDENT_SRC_DEVICEINFO_H

#include <algorithm>
#include <cstring>
#include <string_view>
#include <type_traits>

// This type and the following one must remain POD types to keep their
// layouts as intended! No user defined CTORs, etc.
struct DeviceInfoString {
  char data[64];

  [[nodiscard]] bool Validate() noexcept {
    if (std::memchr(data, 0xFF, sizeof(data)) != nullptr) {
      Clear();
      return false;
    }
    return true;
  }

  void Clear() noexcept {
    std::fill_n(data, sizeof(data), 0);
  }

  void Set(std::string_view str) noexcept {
    const auto copied = str.copy(data, sizeof(data) - 1);
    std::fill_n(data + copied, sizeof(data) - copied, '\0');
  }

  [[nodiscard]] std::string_view Get() const noexcept {
    std::string_view sv{data, sizeof(data)};
    using std::operator""sv;
    const auto idx = sv.find_first_of("\0\xFF"sv);
    if (idx != std::string_view::npos) {
      sv = sv.substr(0, idx);
    }
    return sv;
  }

  [[nodiscard]] std::uint8_t Sum() const noexcept {
    std::uint8_t sum{};
    for (char c : data) {
      sum += static_cast<std::uint8_t>(c);
    }
    return sum;
  }
};

/*
 * This is the structure containing the info to store.
 *
 * Each of the strings in this structure is assumed to be null-terminated.
 */
struct DeviceInfoBlock {
  DeviceInfoString mfg;
  DeviceInfoString name;
  DeviceInfoString ver;
  DeviceInfoString date;
  DeviceInfoString part;
  DeviceInfoString mfgserial;
  DeviceInfoString user1;
  DeviceInfoString user2;
  DeviceInfoString user3;
  DeviceInfoString user4;

  // This checksum field is calculated by summing up all of the previous bytes
  // in this structure. It's crude, but should be good enough for such a basic
  // device.
  std::uint8_t checksum;

  // If any field was not valid and was updated, returns false.
  [[nodiscard]] bool Validate() noexcept {
    bool ok = true;
    ok &= mfg.Validate();
    ok &= name.Validate();
    ok &= ver.Validate();
    ok &= date.Validate();
    ok &= part.Validate();
    ok &= mfgserial.Validate();
    ok &= user1.Validate();
    ok &= user3.Validate();
    ok &= user2.Validate();
    ok &= user4.Validate();
    return ok;
  }

  // Computes (but does not update!) the checksum.
  [[nodiscard]] std::uint8_t ComputeChecksum() const noexcept {
    std::uint8_t sum{};
    sum += mfg.Sum();
    sum += name.Sum();
    sum += ver.Sum();
    sum += date.Sum();
    sum += part.Sum();
    sum += mfgserial.Sum();
    sum += user1.Sum();
    sum += user2.Sum();
    sum += user3.Sum();
    sum += user4.Sum();
    return sum;
  }

  [[nodiscard]] DeviceInfoString* LookupField(std::string_view key) noexcept {
    using std::operator""sv;

    if (key == "MFG"sv) return &mfg;
    if (key == "NAME"sv) return &this->name;
    if (key == "VER"sv) return &ver;
    if (key == "DATE"sv) return &date;
    if (key == "PART"sv) return &part;
    if (key == "MFGSERIAL"sv) return &mfgserial;
    if (key == "USER1"sv) return &user1;
    if (key == "USER2"sv) return &user2;
    if (key == "USER3"sv) return &user3;
    if (key == "USER4"sv) return &user4;

    return nullptr;
  }
};

static_assert(std::is_trivial_v<DeviceInfoBlock> &&
              std::is_standard_layout_v<DeviceInfoBlock>,
              "DeviceInfoBlock must be POD!");

#endif  /* PICO_IDENT_SRC_DEVICEINFO_H */
