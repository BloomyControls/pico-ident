/*
 * Raspberry Pi Pico System Identification Unit (pico-ident)
 *
 * Copyright (c) 2022, Bloomy Controls
 * All rights reserved.
 *
 * This software is distributed under the BSD 3-Clause License. See the LICENSE
 * file for the full license terms.
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "AT24CM02.h"
#include "DeviceInfo.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

/*
 * GPIO pins.
 */
#define PIN_SWITCH (13)
#define PIN_WRLOCK_OUT (14)
#define PIN_WRLOCK_IN (15)
#define PIN_SDA (16)
#define PIN_SCL (17)

#define I2C_INST i2c0

static constexpr uint32_t kDeviceInfoAddr{0x0};
static constexpr uint32_t kEdgeCountAddr{0x800};

// Board ID (this is set only once and stored here).
static char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

// Device info data.
static DeviceInfoBlock data;

// EEPROM peripheral.
static AT24CM02 eeprom{I2C_INST, false};

static volatile uint32_t edgecount;
static uint32_t last_edgecount;
static volatile ::absolute_time_t last_edge_time;

static void StoreDeviceInfo() noexcept {
  // TODO: error handling
  eeprom.Write(kDeviceInfoAddr, reinterpret_cast<const uint8_t*>(&data),
               sizeof(data));
}

static void LoadDeviceInfo() noexcept {
  // TODO: error handling
  eeprom.Read(kDeviceInfoAddr, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

// Validates data. If it was invalid, updates the data and writes it back to the
// EEPROM. Should only be called at startup.
static void ValidateDeviceInfo() noexcept {
  if (!data.Validate()) {
    StoreDeviceInfo();
  }
}

static void StoreEdgeCount(uint32_t ec) noexcept {
  // TODO: error handling
  eeprom.Write(kEdgeCountAddr, reinterpret_cast<const uint8_t*>(&ec),
               sizeof(ec));
}

static void LoadEdgeCount() noexcept {
  uint32_t ec;
  eeprom.Read(kEdgeCountAddr, reinterpret_cast<uint8_t*>(&ec), sizeof(ec));
  edgecount = ec;
}

[[nodiscard]] static bool WriteLockEnabled() noexcept {
  return ::gpio_get(PIN_WRLOCK_IN);
}

static void HandleSerialMessage(std::string_view message) noexcept {
  if (message.empty()) {
    return;
  }

  using std::operator""sv;

  const auto punct_idx = message.find_first_of("=?"sv);
  const auto header = message.substr(0, punct_idx);

  if (punct_idx != std::string_view::npos) {
    if (message[punct_idx] == '=') {
      // Message is an assignment to a field.
      if (WriteLockEnabled()) {
        return;
      }

      if (auto field = data.LookupField(header)) {
        const auto val = message.substr(punct_idx + 1);

        const auto is_invalid = [](char c) -> bool {
          return !std::isprint(static_cast<unsigned char>(c));
        };

        if (std::any_of(val.begin(), val.end(), is_invalid)) {
          return;
        }

        field->Set(val);

        data.checksum = data.ComputeChecksum();
        StoreDeviceInfo();
      }
    } else if (message[punct_idx] == '?') {
      // Message is a query.
      if (const auto* field = data.LookupField(header)) {
        std::printf("%s\n", field->Get().data());
      } else if (header == "SERIAL"sv) {
        std::printf("%s\n", board_id);
      } else if (header == "CHECK"sv) {
        if (data.ComputeChecksum() == data.checksum) {
          std::printf("OK\n");
        } else {
          std::printf("ERR\n");
        }
      } else if (header == "EDGECOUNT"sv) {
        std::printf("%u\n", edgecount);
      }
    }
  } else {
    // No punctuation, so the message is a command.
    if (header == "CLEAR"sv) {
      if (!WriteLockEnabled()) {
        data = {};
        StoreDeviceInfo();
      }
    } else if (header == "RESETCOUNT"sv) {
      StoreEdgeCount(0);
      edgecount = 0;
      last_edgecount = 0;
    }
  }
}

int main(void) {
  ::stdio_init_all();

  ::gpio_init(PIN_WRLOCK_OUT);
  ::gpio_set_dir(PIN_WRLOCK_OUT, GPIO_OUT);
  ::gpio_put(PIN_WRLOCK_OUT, 1);

  ::gpio_init(PIN_WRLOCK_IN);
  ::gpio_set_dir(PIN_WRLOCK_IN, GPIO_IN);
  ::gpio_pull_down(PIN_WRLOCK_IN);

  // Make write lock pins available to picotool.
  bi_decl(bi_2pins_with_names(PIN_WRLOCK_IN, "Write lock in", PIN_WRLOCK_OUT,
                              "Write lock out"));

  ::i2c_init(I2C_INST, 1'000'000);
  ::gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
  ::gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);

  // Make the I2C pins available to picotool.
  bi_decl(bi_2pins_with_func(PIN_SDA, PIN_SCL, GPIO_FUNC_I2C));

  ::gpio_init(PIN_SWITCH);
  ::gpio_set_dir(PIN_SWITCH, GPIO_IN);
  ::gpio_pull_up(PIN_SWITCH);

  // Make the lid switch pin available to picotool.
  bi_decl(bi_1pin_with_name(PIN_SWITCH, "Lid switch (active low)"));

  // Load device info and make sure it's valid.
  LoadDeviceInfo();
  ValidateDeviceInfo();

  LoadEdgeCount();
  // check for blank EEPROM
  if (edgecount == 0xFFFFFFFF) {
    edgecount = 0;
    StoreEdgeCount(0);
  }

  // Get the board ID (we only need to do this once)
  ::pico_get_unique_board_id_string(board_id, sizeof(board_id));

  last_edgecount = edgecount;
  last_edge_time = ::get_absolute_time();

  // Falling edges increase the edge count.
  ::gpio_set_irq_enabled_with_callback(
      PIN_SWITCH, 0, GPIO_IRQ_EDGE_FALL,
      [](uint, uint32_t) {
        const auto now = ::get_absolute_time();
        const auto diff = ::absolute_time_diff_us(last_edge_time, now);
        // 50ms debounce
        if (diff >= 50'000) {
          last_edge_time = now;
          edgecount = edgecount + 1;
        }
      });

  char rdbuf[512] = {0};
  size_t idx = 0;
  int c;
  while (1) {
    c = ::getchar_timeout_us(10);
    if (c == PICO_ERROR_TIMEOUT) {
      if (edgecount != last_edgecount) {
        StoreEdgeCount(last_edgecount = edgecount);
      }
    }

    // If it's a return character, handle the message. If not, add it to the
    // buffer so long as it's valid.
    if (c == '\r') {
      rdbuf[idx] = '\0';
      idx = 0;
      HandleSerialMessage(rdbuf);
    } else if (std::isprint(c)) {
      rdbuf[idx] = c;
      idx = (idx + 1) % sizeof(rdbuf);
    }
  }
}
