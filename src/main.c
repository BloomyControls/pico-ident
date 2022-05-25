/*
 * Raspberry Pi Pico System Identification Unit (pico-ident)
 *
 * Copyright (c) 2022, Bloomy Controls
 * All rights reserved.
 *
 * This software is distributed under the BSD 3-Clause License. See the LICENSE
 * file for the full license terms.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

/*
 * GPIO pins for write lock.
 */
#define WRLOCK_OUT (14)
#define WRLOCK_IN (15)

/*
 * Offset 512K from the start of the flash.
 * Must be aligned to a 4096-byte sector.
 *
 * Note that this could cause issues if this program was larger than 512K, but
 * since this program is only ~30K (last I checked), it shouldn't be an issue.
 */
#define FLASH_TARGET_OFFSET (512 * 1024)

/*
 * This is the structure containing the info to store.
 *
 * Each of the strings in this structure is assumed to be null-terminated.
 */
struct device_info {
  char mfg[64];
  char name[64];
  char ver[64];
  char date[64];
  char part[64];
  char mfgserial[64];
  char user1[64];
  char user2[64];
  char user3[64];
  char user4[64];

  // This checksum field is calculated by summing up all of the previous bytes
  // in this structure. It's crude, but should be good enough for such a basic
  // device.
  uint8_t checksum;
};

// Size of the device info structure + additional space to make it a multiple of
// the flash page size (all writes must be whole numbers of pages).
#define DEVINFO_SIZE                                    \
  (((sizeof(struct device_info) / FLASH_PAGE_SIZE) +    \
    !!(sizeof(struct device_info) % FLASH_PAGE_SIZE)) * \
   FLASH_PAGE_SIZE)

// Device info in flash (read-only, you cannot write through this pointer).
const struct device_info* flash_devinfo =
    (const struct device_info*)(XIP_BASE + FLASH_TARGET_OFFSET);

// Board ID (this is set only once and stored here).
char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

/**
 * @brief Commit a device info structure to flash.
 *
 * This function will erase the sector containing the data and then copy the
 * device info data into the cleared space. Note that if the WRLOCK_IN pin is
 * asserted, this function is a no-op.
 *
 * @todo It may not be necessary to erase at all--it is probably enough to
 * simply write over the existing data.
 *
 * @param[in] info a pointer to a device info struct to store
 */
void store_devinfo(const struct device_info* info) {
  if (!gpio_get(WRLOCK_IN)) {
    // TODO: do we even need to erase here? This might be redundant.
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    if (info != NULL) {
      static uint8_t buf[DEVINFO_SIZE] = {0};
      memcpy(buf, info, sizeof(*info));

      ints = save_and_disable_interrupts();
      flash_range_program(FLASH_TARGET_OFFSET, buf, DEVINFO_SIZE);
      restore_interrupts(ints);
    }
  }
}

/**
 * @brief Compute the 8-bit checksum of a device info structure.
 *
 * @param[in] info the structure to use
 *
 * @return The 8-bit checksum of the structure.
 */
uint8_t compute_checksum(const struct device_info* info) {
  if (info != NULL) {
    uint8_t sum = 0;
    const uint8_t* bytes = (const uint8_t*)info;
    for (size_t i = 0; i < (sizeof(info) - 1); ++i) {
      sum += bytes[i];
    }
  }
}

/**
 * @brief Validate the device info in flash and update it if necessary to clear
 * any invalid data. This function is to be run once at boot.
 *
 * On startup with a new pico, it's entirely possible for the flash to be in its
 * erased state (filled with FFs). This, of course, does not equate to
 * a zero-length string. This function checks the device info stored in flash to
 * make sure that it doesn't contain any FFs. If any field does, it will be
 * zeroed out. This case should only arise on a fresh flash.
 */
void validate_devinfo() {
  struct device_info devinfo = *flash_devinfo;

  // We can be smart about this: any set field is guaranteed not to contain any
  // FF bytes, as strncpy will have zero-filled it. Any field containing an
  // invalid byte is therefore invalid.
#define VAL_FIELD(field, n)                         \
  do {                                              \
    if (memchr(devinfo.field, 0xFF, (n)) != NULL) { \
      memset(devinfo.field, '\0', (n));             \
    }                                               \
  } while (0)

  VAL_FIELD(mfg, 64);
  VAL_FIELD(name, 64);
  VAL_FIELD(ver, 64);
  VAL_FIELD(date, 64);
  VAL_FIELD(part, 64);
  VAL_FIELD(mfgserial, 64);
  VAL_FIELD(user1, 64);
  VAL_FIELD(user2, 64);
  VAL_FIELD(user3, 64);
  VAL_FIELD(user4, 64);

  if (memcmp(&devinfo, flash_devinfo, sizeof(devinfo)) != 0) {
    devinfo.checksum = compute_checksum(&devinfo);
    store_devinfo(&devinfo);
  }
}

/**
 * @brief Handle and respond to serial messages.
 *
 * The input string may be modified to add a null terminator to trim a value to
 * length, so the caller should not rely on the string containing no embedded
 * nulls.
 *
 * @param[in,out] msg a null-terminated string containing the message to handle
 */
void handle_msg(char* msg) {
  if (msg == NULL) return;

  static struct device_info wrinfo;

  // This macro is used to define set/get commands for specific fields. This is
  // just to avoid the massive block of ugly code we had here before, at the
  // expense of a tiny bit of maintainability.
  // GCC will optimize out the strlens here (in fact when confirming this I was
  // unable to make it *not* optimize them out).
#define RW_FIELD(fname, field, len)                                 \
  do {                                                              \
    if (strncmp(msg, (fname "="), strlen(fname "=")) == 0) {        \
      msg += strlen(fname "=");                                     \
      msg[strnlen(msg, (len)-1)] = '\0';                            \
      wrinfo = *flash_devinfo;                                      \
      strncpy(wrinfo.field, msg, (len));                            \
      wrinfo.checksum = compute_checksum(&wrinfo);                  \
      store_devinfo(&wrinfo);                                       \
      return;                                                       \
    } else if (strncmp(msg, (fname "?"), strlen(fname "?")) == 0) { \
      printf("%s\n", flash_devinfo->field);                         \
      return;                                                       \
    }                                                               \
  } while (0)

  RW_FIELD("MFG", mfg, 64);
  RW_FIELD("NAME", name, 64);
  RW_FIELD("VER", ver, 64);
  RW_FIELD("DATE", date, 64);
  RW_FIELD("PART", part, 64);
  RW_FIELD("MFGSERIAL", mfgserial, 64);
  RW_FIELD("USER1", user1, 64);
  RW_FIELD("USER2", user2, 64);
  RW_FIELD("USER3", user3, 64);
  RW_FIELD("USER4", user4, 64);

  if (strncmp(msg, "SERIAL?", 7) == 0) {
    printf("%s\n", board_id);
    return;
  }

  if (strncmp(msg, "CLEAR", 5) == 0) {
    memset(&wrinfo, 0, sizeof(wrinfo));
    store_devinfo(&wrinfo);
    return;
  }

  if (strncmp(msg, "CHECK?", 6) == 0) {
    uint8_t sum = compute_checksum(flash_devinfo);
    if (sum == flash_devinfo->checksum) {
      printf("OK\n");
    } else {
      printf("ERR\n");
    }
    return;
  }
}

int main(void) {
  stdio_init_all();

  gpio_init(WRLOCK_OUT);
  gpio_set_dir(WRLOCK_OUT, GPIO_OUT);
  gpio_put(WRLOCK_OUT, 1);

  gpio_init(WRLOCK_IN);
  gpio_set_dir(WRLOCK_IN, GPIO_IN);
  gpio_pull_down(WRLOCK_IN);

  // Make write lock pins available to picotool.
  bi_decl(bi_2pins_with_names(WRLOCK_IN, "Write lock in", WRLOCK_OUT,
                              "Write lock out"));

  // Make sure the data in flash is valid
  validate_devinfo();

  // Get the board ID (we only need to do this once)
  pico_get_unique_board_id_string(board_id, sizeof(board_id));

  char rdbuf[512] = {0};
  size_t idx = 0;
  int c;
  while (1) {
    c = getchar_timeout_us(10);
    if (c == PICO_ERROR_TIMEOUT) continue;

    // If it's a return character, handle the message. If not, add it to the
    // buffer so long as it's valid.
    if (c == '\r') {
      rdbuf[idx] = '\0';
      idx = 0;
      handle_msg(rdbuf);
    } else if (isprint(c)) {
      rdbuf[idx] = c;
      idx = (idx + 1) % sizeof(rdbuf);
    }
  }
}
