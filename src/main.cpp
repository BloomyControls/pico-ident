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
#define PIN_LED PICO_DEFAULT_LED_PIN

#define I2C_INST i2c0

// How long a low pulse must be to be counted.
#ifdef CONFIG_MIN_PULSE_WIDTH_US
#define MIN_PULSE_WIDTH_US (int64_t(CONFIG_MIN_PULSE_WIDTH_US))
#else
#define MIN_PULSE_WIDTH_US (100'000ULL)
#endif  // CONFIG_MIN_PULSE_WIDTH_US

static_assert(MIN_PULSE_WIDTH_US >= 10ULL,
              "Minimum pulse width must be at least 10us!");

static constexpr std::uint32_t kDeviceInfoAddr{0x0};
static constexpr std::uint32_t kPulseCountAddr{0x800};

// Board ID (this is set only once and stored here).
static char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

// Device info data.
static DeviceInfoBlock data;

// EEPROM peripheral.
static AT24CM02 eeprom{I2C_INST, true};

static volatile std::uint32_t pulsecount;
static std::uint32_t last_pulsecount;

[[noreturn]] static void Panic() noexcept {
  while (1) {
    ::gpio_xor_mask(1U << PIN_LED);
    ::sleep_ms(250);
  }
}

static void StoreDeviceInfo() noexcept {
  if (!eeprom.Write(kDeviceInfoAddr,
                    reinterpret_cast<const std::uint8_t*>(&data),
                    sizeof(data))) {
    Panic();
  }
}

static void LoadDeviceInfo() noexcept {
  if (!eeprom.Read(kDeviceInfoAddr, reinterpret_cast<std::uint8_t*>(&data),
                   sizeof(data))) {
    Panic();
  }
}

// Validates data. If it was invalid, updates the data and writes it back to the
// EEPROM. Should only be called at startup.
static void ValidateDeviceInfo() noexcept {
  if (!data.Validate()) {
    data.checksum = data.ComputeChecksum();
    StoreDeviceInfo();
  }
}

static void StorePulseCount(std::uint32_t pc) noexcept {
  if (!eeprom.Write(kPulseCountAddr, reinterpret_cast<const std::uint8_t*>(&pc),
                    sizeof(pc))) {
    Panic();
  }
}

static void LoadPulseCount() noexcept {
  std::uint32_t pc;
  if (!eeprom.Read(kPulseCountAddr, reinterpret_cast<std::uint8_t*>(&pc),
                   sizeof(pc))) {
    Panic();
  }
  pulsecount = pc;
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
      } else if (header == "PULSECOUNT"sv) {
        std::printf("%u\n", pulsecount);
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
      StorePulseCount(0);
      pulsecount = 0;
      last_pulsecount = 0;
    }
  }
}

int main(void) {
  ::stdio_init_all();

  ::gpio_init(PIN_LED);
  ::gpio_set_dir(PIN_LED, GPIO_OUT);
  ::gpio_put(PIN_LED, 1);

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

  LoadPulseCount();
  // check for blank EEPROM
  if (pulsecount == 0xFFFFFFFF) {
    pulsecount = 0;
    StorePulseCount(0);
  }

  // Get the board ID (we only need to do this once)
  ::pico_get_unique_board_id_string(board_id, sizeof(board_id));

  last_pulsecount = pulsecount;
  static volatile ::absolute_time_t falling_edge_time = ::get_absolute_time();
  static volatile bool pulse_started = false;

  // When we see a falling edge, store the time that happened. When we see
  // a rising edge, see how long the pulse was, and if it was long enough,
  // increment the edge count. Else, we wait for the next falling edge.
  ::gpio_set_irq_enabled_with_callback(
      PIN_SWITCH, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true,
      [](::uint, std::uint32_t event_mask) {
        const auto now = ::get_absolute_time();
        if (!pulse_started && (event_mask & GPIO_IRQ_EDGE_FALL)) {
          pulse_started = true;
          falling_edge_time = now;
        } else if (pulse_started && (event_mask & GPIO_IRQ_EDGE_RISE)) {
          const auto diff = ::absolute_time_diff_us(falling_edge_time, now);
          // Look for low pulses wide enough to be counted.
          if (diff >= MIN_PULSE_WIDTH_US) {
            // XXX: should this be set to false outside this condition?
            pulse_started = false;
            pulsecount = pulsecount + 1;
          }
        }
      });

  char rdbuf[512] = {0};
  std::size_t idx = 0;
  int c;
  while (1) {
    c = ::getchar_timeout_us(10);
    if (c == PICO_ERROR_TIMEOUT) {
      if (pulsecount != last_pulsecount) {
        StorePulseCount(last_pulsecount = pulsecount);
      }
      continue;
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
