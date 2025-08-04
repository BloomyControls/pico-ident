#include "AT24CM02.h"

#include <algorithm>

constexpr static uint32_t kAddrMask{(1U << 18U) - 1U};

AT24CM02::AT24CM02(i2c_inst_t* i2c, bool addr_pin) noexcept
    : i2c_{i2c}, addr_pin_(addr_pin ? 1U : 0U) {}

bool AT24CM02::Write(uint32_t addr, const uint8_t* buf,
                     size_t len) const noexcept {
  if (!i2c_ || !buf || len == 0 || (addr & ~kAddrMask)) {
    return false;
  }

  if (addr + len >= kPages * kBytesPerPage) {
    return false;
  }

  size_t page_remain = kBytesPerPage - (addr % kBytesPerPage);

  if (len < page_remain) {
    page_remain = len;
  }

  while (len > 0) {
    const uint8_t i2c_addr = (0xA0 | (addr_pin_ << 3U) | (addr >> 15U)) >> 1U;
    uint8_t addr_bytes[2];

    addr_bytes[0] = (addr & 0x0FF00) >> 8;
    addr_bytes[1] = addr & 0x000FF;

    if (::i2c_write_burst_blocking(i2c_, i2c_addr, addr_bytes,
                                   sizeof(addr_bytes)) == PICO_ERROR_GENERIC) {
      return false;
    }

    if (::i2c_write_blocking(i2c_, i2c_addr, buf, page_remain, false) ==
        PICO_ERROR_GENERIC) {
      return false;
    }

    // this could be replaced with ACK polling if we hate waiting for the max
    // write time
    sleep_ms(10);

    addr += page_remain;
    buf += page_remain;
    len -= page_remain;

    page_remain = std::min(len, kBytesPerPage);
  }

  return true;
}

bool AT24CM02::Read(uint32_t addr, uint8_t* buf, size_t len) const noexcept {
  if (!i2c_ || !buf || len == 0 || (addr & ~kAddrMask)) {
    return false;
  }

  if (addr + len >= kPages * kBytesPerPage) {
    return false;
  }

  const uint8_t i2c_addr = (0xA0 | (addr_pin_ << 3U) | (addr >> 15U)) >> 1U;
  uint8_t addr_bytes[2];

  addr_bytes[0] = (addr & 0x0FF00) >> 8;
  addr_bytes[1] = addr & 0x000FF;

  if (::i2c_write_blocking(i2c_, i2c_addr, addr_bytes, sizeof(addr_bytes),
                           true) == PICO_ERROR_GENERIC) {
    return false;
  }

  if (::i2c_read_blocking(i2c_, i2c_addr, buf, len, false) ==
      PICO_ERROR_GENERIC) {
    return false;
  }

  return true;
}
