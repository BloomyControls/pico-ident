#ifndef PICO_IDENT_SRC_AT24CM02_H
#define PICO_IDENT_SRC_AT24CM02_H

#include <cstddef>
#include <cstdint>

#include "hardware/i2c.h"

class AT24CM02 {
 public:
  static constexpr std::size_t kPages{1024U};
  static constexpr std::size_t kBytesPerPage{256U};

  explicit AT24CM02(::i2c_inst_t* i2c, bool addr_pin) noexcept;

  explicit AT24CM02(std::nullptr_t, bool) = delete;

  bool Write(std::uint32_t addr, const std::uint8_t* buf,
             std::size_t len) const noexcept;

  bool Read(std::uint32_t addr, std::uint8_t* buf,
            std::size_t len) const noexcept;

 private:
  const std::uint8_t addr_pin_;
  ::i2c_inst_t* i2c_;
};

#endif /* PICO_IDENT_SRC_AT24CM02_H */
