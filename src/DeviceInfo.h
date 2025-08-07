/*
 * Device Info Data Structures
 *
 * Copyright (c) 2025, Bloomy Controls, Inc. All rights reserved.
 * Use of this source code is governed by a BSD-3-clause license that can be
 * found in the LICENSE file or at https://opensource.org/license/BSD-3-Clause
 */

#ifndef PICO_IDENT_SRC_DEVICEINFO_H
#define PICO_IDENT_SRC_DEVICEINFO_H

#include <algorithm>
#include <cstring>
#include <string_view>
#include <type_traits>

/**
 * @brief Device info block string. Handles the 64 character nul-terminated
 * string and has utilities for computing the checksum, validating/clearing the
 * flash, etc.
 *
 * When the string is set, it will be truncated to at most 63 characters and
 * will be stored with a null terminator. All unused bytes after the string are
 * guaranteed to be 0.
 *
 * @warning This structure MUST remain a POD type! No constructors, no
 * destructors, no complicated stuff. It is directly stored to the EEPROM, so
 * its layout must remain the same.
 */
struct DeviceInfoString {
  /// String data.
  char data[64];

  /**
   * @brief Ensure the string data is valid and fix it if it's not.
   *
   * This is used to detect a blank EEPROM (indicated by the presence of FF
   * bytes) and zero the EEPROM if necessary.
   *
   * @return Whether the data was valid. If this function returns false, the
   * data was zeroed and should be written back to the EEPROM.
   */
  [[nodiscard]] bool Validate() noexcept {
    if (std::memchr(data, 0xFF, sizeof(data)) != nullptr) {
      Clear();
      return false;
    }
    return true;
  }

  /// Clear (zero) the string.
  void Clear() noexcept {
    std::fill_n(data, sizeof(data), 0);
  }

  /**
   * @brief Set the string's contents.
   *
   * If the string provided is longer than 63 bytes, it will be truncated. If it
   * is shorter, all bytes after the string are set to zero.
   *
   * @param[in] str new string to set
   */
  void Set(std::string_view str) noexcept {
    const auto copied = str.copy(data, sizeof(data) - 1);
    std::fill_n(data + copied, sizeof(data) - copied, '\0');
  }

  /**
   * @brief Get the contents of the field.
   *
   * @return A view of the contained string.
   */
  [[nodiscard]] std::string_view Get() const noexcept {
    std::string_view sv{data, sizeof(data)};
    using std::operator""sv;
    const auto idx = sv.find_first_of("\0\xFF"sv);
    if (idx != std::string_view::npos) {
      sv = sv.substr(0, idx);
    }
    return sv;
  }

  /**
   * @brief Compute the 8-bit sum of the characters in the string.
   *
   * @return The sum of the bytes in the string.
   */
  [[nodiscard]] std::uint8_t Sum() const noexcept {
    std::uint8_t sum{};
    for (char c : data) {
      sum += static_cast<std::uint8_t>(c);
    }
    return sum;
  }
};

/**
 * @brief This is the data block stored to the EEPROM.
 *
 * @warning This type must remain POD! Its size and alignment are important, as
 * it is directly written to the EEPROM.
 */
struct DeviceInfoBlock {
  /// Manufacturer (MFG).
  DeviceInfoString mfg;
  /// Name (NAME).
  DeviceInfoString name;
  /// Version (VER).
  DeviceInfoString ver;
  /// Date (DATE).
  DeviceInfoString date;
  /// Part number (PART).
  DeviceInfoString part;
  /// Custom serial number (MFGSERIAL); not to be confused with the Pico's
  /// unique 64-bit ID.
  DeviceInfoString mfgserial;
  /// User field 1 (USER1).
  DeviceInfoString user1;
  /// User field 2 (USER2).
  DeviceInfoString user2;
  /// User field 3 (USER3).
  DeviceInfoString user3;
  /// User field 4 (USER4).
  DeviceInfoString user4;

  /// Checksum calculated by summing up all the bytes of the fields in the
  /// struct. Crude, but good enough. Must be updated before writing to the
  /// EEPROM.
  std::uint8_t checksum;

  /**
   * @brief Checks for invalid fields (containing FF bytes), zeroing any fields
   * found to be invalid.
   *
   * @return Whether or not the data was valid. If this function returns false,
   * the CRC should be updated and the data should be written back to the
   * EEPROM.
   */
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

  /**
   * @brief Computes (but does not update!) the checksum of the data. Can be
   * used to recalculate the checksum before writing or to compute it for
   * comparison with the expected value stored in the EEPROM.
   *
   * @return The computed checksum.
   */
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

  /**
   * @brief Get a pointer to a field of this structure by a named key.
   *
   * These keys are used for parsing serial messages.
   *
   * @param[in] key the key for the field to lookup (e.g., "NAME")
   *
   * @return Pointer to the associated field or nullptr if an unknown key is
   * given.
   */
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
