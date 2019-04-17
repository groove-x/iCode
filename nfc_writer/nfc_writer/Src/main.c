#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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

  // write data to selected tag
  uint16_t total = 0;
  for (int j = 0; j < BLOCK_LENGTH; j++) {
    uint8_t writeBuf[BLOCK_SIZE] = {j, j+1, j+2, j+3};
    do {
      ret = rfalNfvPollerWriteSingleBlock(RFAL_NFCV_REQ_FLAG_DEFAULT, NULL, j, writeBuf, BLOCK_SIZE);
      if (ret != ERR_NONE) {
        usleep(100*1000);
        continue;
      }
      total += BLOCK_SIZE;
    } while(ret == ERR_NONE);
  }

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
