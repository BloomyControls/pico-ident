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

// On startup, it's possible for the flash to be filled with 0xFF (this happens
// when the flash is erased). Obviously this doesn't work well when trying to
// read null-terminated strings from it, so we can check each field of the
// device info stored in flash and clear it if it's full of Fs.
void validate_devinfo() {
  struct device_info devinfo = *flash_devinfo;

  if (devinfo.mfg[0] == 0xFF) devinfo.mfg[0] = '\0';
  if (devinfo.name[0] == 0xFF) devinfo.name[0] = '\0';
  if (devinfo.ver[0] == 0xFF) devinfo.ver[0] = '\0';
  if (devinfo.date[0] == 0xFF) devinfo.date[0] = '\0';
  if (devinfo.part[0] == 0xFF) devinfo.part[0] = '\0';
  if (devinfo.mfgserial[0] == 0xFF) devinfo.mfgserial[0] = '\0';
  if (devinfo.user1[0] == 0xFF) devinfo.user1[0] = '\0';
  if (devinfo.user2[0] == 0xFF) devinfo.user2[0] = '\0';
  if (devinfo.user3[0] == 0xFF) devinfo.user3[0] = '\0';
  if (devinfo.user4[0] == 0xFF) devinfo.user4[0] = '\0';

  if (memcmp(&devinfo, flash_devinfo, sizeof(devinfo)) != 0) {
    store_devinfo(&devinfo);
  }
}

// handle a null-terminated message
void handle_msg(char* msg) {
  if (msg == NULL) return;

  static struct device_info wrinfo;

  if (strncmp(msg, "MFG=", 4) == 0) {
    msg += 4;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.mfg, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "MFG?", 4) == 0) {
    printf("%s\n", flash_devinfo->mfg);
  } else if (strncmp(msg, "NAME=", 5) == 0) {
    msg += 5;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.name, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "NAME?", 5) == 0) {
    printf("%s\n", flash_devinfo->name);
  } else if (strncmp(msg, "VER=", 4) == 0) {
    msg += 4;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.ver, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "VER?", 4) == 0) {
    printf("%s\n", flash_devinfo->ver);
  } else if (strncmp(msg, "DATE=", 5) == 0) {
    msg += 5;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.date, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "DATE?", 5) == 0) {
    printf("%s\n", flash_devinfo->date);
  } else if (strncmp(msg, "PART=", 5) == 0) {
    msg += 5;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.part, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "PART?", 5) == 0) {
    printf("%s\n", flash_devinfo->part);
  } else if (strncmp(msg, "MFGSERIAL=", 10) == 0) {
    msg += 10;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.mfgserial, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "MFGSERIAL?", 10) == 0) {
    printf("%s\n", flash_devinfo->mfgserial);
  } else if (strncmp(msg, "SERIAL?", 7) == 0) {
    printf("%s\n", board_id);
  } else if (strncmp(msg, "USER1=", 6) == 0) {
    msg += 6;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.user1, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "USER1?", 6) == 0) {
    printf("%s\n", flash_devinfo->user1);
  } else if (strncmp(msg, "USER2=", 6) == 0) {
    msg += 6;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.user2, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "USER2?", 6) == 0) {
    printf("%s\n", flash_devinfo->user2);
  } else if (strncmp(msg, "USER3=", 6) == 0) {
    msg += 6;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.user3, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "USER3?", 6) == 0) {
    printf("%s\n", flash_devinfo->user3);
  } else if (strncmp(msg, "USER4=", 6) == 0) {
    msg += 6;
    msg[strnlen(msg, 63)] = '\0';
    wrinfo = *flash_devinfo;
    strncpy(wrinfo.user4, msg, 64);
    store_devinfo(&wrinfo);
  } else if (strncmp(msg, "USER4?", 6) == 0) {
    printf("%s\n", flash_devinfo->user4);
  } else if (strncmp(msg, "CLEAR", 5) == 0) {
    memset(&wrinfo, 0, sizeof(wrinfo));
    store_devinfo(&wrinfo);
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
