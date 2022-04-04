#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
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

// Compute 8-bit checksum of a device info structure.
uint8_t compute_checksum(const struct device_info* info) {
  if (info != NULL) {
    uint8_t sum = 0;
    const uint8_t* bytes = (const uint8_t*)info;
    for (size_t i = 0; i < (sizeof(info) - 1); ++i) {
      sum += bytes[i];
    }
  }
}

// On startup, it's possible for the flash to be filled with 0xFF (this happens
// when the flash is erased). Obviously this doesn't work well when trying to
// read null-terminated strings from it, so we can check each field of the
// device info stored in flash and clear it if it's full of Fs.
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

// handle a null-terminated message
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

  validate_devinfo();

  // Get the board ID (we only need to do this once)
  pico_get_unique_board_id_string(board_id, sizeof(board_id));

  char rdbuf[512] = {0};
  size_t idx = 0;
  int c;
  while (1) {
    c = getchar_timeout_us(10);
    if (c == PICO_ERROR_TIMEOUT) continue;

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
