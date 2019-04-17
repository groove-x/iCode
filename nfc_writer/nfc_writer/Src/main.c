#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "rfal_analogConfig.h"
#include "rfal_nfcv.h"

#define DEV_LIMIT 10
#define BLOCK_LENGTH 4
#define BLOCK_SIZE 4
#define DATA_SIZE (BLOCK_LENGTH*BLOCK_SIZE)

static bool initialize(void);
static void start(void);

bool initialize(void) {
  if (gpio_init() != ERR_NONE) {
    return false;
  }
  if (spi_init() != ERR_NONE) {
    return false;
  }
  if (interrupt_init() != ERR_NONE) {
    return false;
  }

  rfalAnalogConfigInitialize();
  rfalInitialize();

  if (rfalNfcvPollerInitialize() != ERR_NONE) {
    printf("failed to poller Initialize\n");
    return false;
  }

  return true;
}

ReturnCode find_device(rfalNfcvListenDevice *devices, uint8_t *size) {
  ReturnCode ret = ERR_NONE;
  rfalNfcvInventoryRes invRes;

  ret = rfalNfcvPollerCheckPresence(&invRes);
  if (ret != ERR_NONE) {
    printf("failed to check presence\n");
    return ret;
  }

  ret = rfalNfcvPollerCollisionResolution(DEV_LIMIT, devices, size);
  if (ret != ERR_NONE) {
    printf("failed to poll collision resolution: %d\n", ret);
    return ret;
  }

  if (*size == 0) {
    printf("device not found\n");
    return ERR_NOTFOUND;
  }

  return ret;
}

void start(void) {
  ReturnCode ret;
  rfalNfcvListenDevice nfcvDevList[DEV_LIMIT];
  uint8_t devCnt = 0;

  ret = rfalFieldOnAndStartGT();
  if (ret != ERR_NONE) {
    printf("failed to start\n");
    return;
  }

  if (find_device(nfcvDevList, &devCnt) != ERR_NONE) {
    return;
  }

  if (devCnt > 1) {
    printf("found multiple tags: %d\n", devCnt);
    return;
  }

  // select tag
  uint8_t *uid = nfcvDevList[0].InvRes.UID;
  ret = rfalNfvPollerSelect(RFAL_NFCV_REQ_FLAG_DEFAULT, uid);
  if (ret != ERR_NONE) {
    printf("failed to select tag: %d\n", ret);
    return;
  }

  // write data
  uint8_t writeBuf[DATA_SIZE] = {0x00};
  for (int j = 0; j < DATA_SIZE; j++) {
    writeBuf[j] = (uint8_t)j;
  }
  uint16_t total = 0;
  for (int j = 0; j < BLOCK_LENGTH; j++) {
    ret = rfalNfvPollerWriteSingleBlock(RFAL_NFCV_REQ_FLAG_DEFAULT, uid, j, writeBuf+total, BLOCK_SIZE);
    if (ret != ERR_NONE) {
      printf("failed to write data: result %d\n", ret, j);
      return;
    }
    total += BLOCK_SIZE;
  }

  // read data
  uint8_t readBuf[DATA_SIZE] = {0x00};
  uint16_t size = 0;
  ret = rfalNfvPollerReadMultipleBlocks(RFAL_NFCV_REQ_FLAG_DEFAULT, uid, 0, BLOCK_LENGTH, readBuf, DATA_SIZE, &size);
  if (ret != ERR_NONE) {
    printf("failed to read blocks: %d\n", ret);
    return;
  }

  // print info
  printf("UID: ");
  for (int j = 0; j < 8; j++) {
    printf("%02x", *(uid + j));
  }
  printf("  DATA: ");
  for (int j = 0; j < DATA_SIZE; j++) {
    if (j >= size) {
      printf("00 ", 0x00);
    } else {
      printf("%02x ", *(readBuf + j));
    }
  }
  printf("  LENGTH: %d\n", size);

  if (rfalFieldOff() != ERR_NONE) {
    printf("failed to stop\n");
    return;
  }
}

int main(void) {
  int ret = initialize();
  if (ret != true) {
    printf("failed to init hardware\n");
    return 1;
  }

  start();

  return 0;
}
