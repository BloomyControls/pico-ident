/*
 * AT24CM02 I2C Driver
 *
 * Copyright (c) 2025, Bloomy Controls, Inc. All rights reserved.
 * Use of this source code is governed by a BSD-3-clause license that can be
 * found in the LICENSE file or at https://opensource.org/license/BSD-3-Clause
 */

#ifndef PICO_IDENT_SRC_AT24CM02_H
#define PICO_IDENT_SRC_AT24CM02_H

#include <cstddef>
#include <cstdint>

#include "hardware/i2c.h"

/**
 * @brief Driver for AT24CM02 I2C EEPROM chip.
 */
class AT24CM02 {
 public:
  /// Total number of pages in flash.
  static constexpr std::size_t kPages{1024U};
  /// Number of bytes per page.
  static constexpr std::size_t kBytesPerPage{256U};

  /**
   * @brief Constructor.
   *
   * @param[in] i2c I2C instance to use
   * @param[in] addr_pin state of the A2 pin on the EEPROM chip (high or low)
   */
  explicit AT24CM02(::i2c_inst_t* i2c, bool addr_pin) noexcept;

  explicit AT24CM02(std::nullptr_t, bool) = delete;

  /**
   * @brief Write data to the EEPROM. Does not need to be page-aligned, and may
   * be longer than a page.
   *
   * @param[in] addr 18-bit memory address to write to
   * @param[in] buf data to write to memory
   * @param[in] len length of data in bytes
   *
   * @return Whether the write was successful.
   */
  bool Write(std::uint32_t addr, const std::uint8_t* buf,
             std::size_t len) const noexcept;

  /**
   * @brief Read data from the EEPROM. Does not need to be page-aligned, and may
   * be longer than a page.
   *
   * @param[in] addr 18-bit memory address to write to
   * @param[out] buf buffer to write data into
   * @param[in] len number of bytes to read
   *
   * @return Whether the read was successful.
   */
  bool Read(std::uint32_t addr, std::uint8_t* buf,
            std::size_t len) const noexcept;

 private:
  /// State of the address pin on the chip.
  const std::uint8_t addr_pin_;
  /// I2C instance.
  ::i2c_inst_t* i2c_;
};

#endif /* PICO_IDENT_SRC_AT24CM02_H */
