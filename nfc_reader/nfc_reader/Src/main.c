#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "rfal_analogConfig.h"
#include "rfal_nfcv.h"

#define DEV_LIMIT 10
#define READ_BLOCKS 4
#define BLOCK_SIZE 5
#define READ_SIZE (READ_BLOCKS*BLOCK_SIZE)

static bool initialize(void);
static void readNFCVSingleBlock(void);

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

  ret = rfalFieldOnAndStartGT();
  if (ret != ERR_NONE) {
    printf("failed to start\n");
    return ret;
  }

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

void readNFCVSingleBlock(void) {
  ReturnCode ret;
  rfalNfcvListenDevice nfcvDevList[DEV_LIMIT];
  uint8_t devCnt = 0;
  uint16_t rxBufLen = 32;

  if (find_device(nfcvDevList, &devCnt) != ERR_NONE) {
    return;
  }

  for (int i = 0; i < devCnt; i++) {
    uint8_t *uid = nfcvDevList[i].InvRes.UID;

    ret = rfalNfvPollerSelect(RFAL_NFCV_REQ_FLAG_DEFAULT, uid);
    if (ret != ERR_NONE) {
      printf("failed to select tag: %d\n", ret);
      return;
    }

    // read data
    uint8_t buff[READ_SIZE] = {0x00};
    uint16_t size = 0;
    ret = rfalNfvReadMultipleBlocks(RFAL_NFCV_REQ_FLAG_DEFAULT, uid, 0, READ_BLOCKS, buff, READ_SIZE, &size);
    if (ret != ERR_NONE) {
      printf("failed to read blocks: %d\n", ret);
      continue;
    }

    printf("DATA: ");
    for (int j = 0; j < READ_SIZE; j++) {
      if (j >= size) {
        printf("00 ", 0x00);
      } else {
        printf("%02x ", *(buff + j));
      }
    }
    printf("  LENGTH: %d\n", size);
    usleep(1000*1000);
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

  // read single block
  readNFCVSingleBlock();

  return 0;
}
