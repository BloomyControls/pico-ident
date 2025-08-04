#ifndef PICO_IDENT_SRC_AT24CM02_H
#define PICO_IDENT_SRC_AT24CM02_H

#include "hardware/i2c.h"

#include <cstddef>
#include <cstdint>

class AT24CM02 {
 public:
  static constexpr size_t kPages{1024U};
  static constexpr size_t kBytesPerPage{256U};

  explicit AT24CM02(i2c_inst_t* i2c, bool addr_pin) noexcept;

  explicit AT24CM02(std::nullptr_t, bool) = delete;

  bool Write(uint32_t addr, const uint8_t* buf, size_t len) const noexcept;

  bool Read(uint32_t addr, uint8_t* buf, size_t len) const noexcept;

 private:
  const uint8_t addr_pin_;
  i2c_inst_t* i2c_;
};

#endif  /* PICO_IDENT_SRC_AT24CM02_H */
