#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rfal_analogConfig.h"
#include "rfal_nfcDep.h"
#include "utils.h"

#define BUF_LEN 255

static union {
  // rfalIsoDepApduBufFormat isoDepRxBuf;
  rfalNfcDepBufFormat nfcDepRxBuf;
  uint8_t rxBuf[BUF_LEN];
} gRxBuf;
uint16_t gRxLen;

rfalNfcDepDevice gNfcDepDev;
static uint8_t NFCID3[] = {0x01, 0xFE, 0x03, 0x04, 0x05,
                           0x06, 0x07, 0x08, 0x09, 0x0A};
bool gIsRxChaining;

int main(void) {
  ReturnCode err;

  // init
  if (gpio_init() != ERR_NONE) {
    return 1;
  }
  if (spi_init() != ERR_NONE) {
    return 2;
  }
  if (interrupt_init() != ERR_NONE) {
    return 3;
  }

  // init listen
  rfalFieldOff();
  int tick = time(NULL);
  err = rfalListenStart(RFAL_LM_MASK_ACTIVE_P2P, NULL, NULL, NULL, gRxBuf.rxBuf,
                        BUF_LEN, &gRxLen);
  if (err != ERR_NONE) {
    printf("failed to listen start: %d\n", err);
    return 4;
  }
  printf("listen start done.\n");

  // wait act rf
  bool detected = false;
  while (time(NULL) - tick < 10) {
    bool dataFlag;
    rfalBitRate bitRate;
    rfalLmState lmst;
    uint8_t hdrLen = RFAL_NFCDEP_LEN_LEN;

    // check initiator sent data
    lmst = rfalListenGetState(&dataFlag, &bitRate);
    printf("waiting activation. STATE: %d, DATAFLAG: %d\n", lmst, dataFlag);
    if ((lmst == RFAL_LM_STATE_IDLE) && dataFlag) {
      // NFC-A case
      if (bitRate == RFAL_BR_106) {
        hdrLen += RFAL_NFCDEP_SB_LEN;
      }

      // check data
      if (rfalNfcDepIsAtrReq(&gRxBuf.rxBuf[hdrLen],
                             (rfalConvBitsToBytes(gRxLen) - hdrLen), NULL)) {
        rfalNfcDepTargetParam param;
        rfalNfcDepListenActvParam rxParam;
        rfalListenSetState((RFAL_BR_106 == bitRate) ? RFAL_LM_STATE_TARGET_A
                                                    : RFAL_LM_STATE_TARGET_F);
        rfalSetMode(RFAL_MODE_LISTEN_ACTIVE_P2P, bitRate, bitRate);
        printf("Activated as AP2P listener device\n");

        // create param
        memcpy(param.nfcid3, NFCID3, RFAL_NFCDEP_NFCID3_LEN);
        param.bst = RFAL_NFCDEP_Bx_NO_HIGH_BR;
        param.brt = RFAL_NFCDEP_Bx_NO_HIGH_BR;
        param.to = RFAL_NFCDEP_WT_TRG_MAX_D11;
        param.ppt = (RFAL_NFCDEP_LR_254 << RFAL_NFCDEP_PP_LR_SHIFT);
        param.GBtLen = 0;
        param.operParam =
            (RFAL_NFCDEP_OPER_FULL_MI_DIS | RFAL_NFCDEP_OPER_EMPTY_DEP_EN |
             RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);
        param.commMode = RFAL_NFCDEP_COMM_ACTIVE;

        rxParam.rxBuf = &gRxBuf.nfcDepRxBuf;
        rxParam.rxLen = &gRxLen;
        rxParam.isRxChaining = &gIsRxChaining;
        rxParam.nfcDepDev = &gNfcDepDev;

        /* ATR_REQ received, trigger NFC-DEP layer to handle activation (sends ATR_RES and handles PSL_REQ)  */
        err = rfalNfcDepListenStartActivation(
            &param, &gRxBuf.rxBuf[hdrLen],
            (rfalConvBitsToBytes(gRxLen) - hdrLen), rxParam);
        if (err != ERR_NONE) {
          printf("failed to listen start activation: %d\n", err);
          return 5;
        } else {
          printf("listen start activation done.\n");
          detected = true;
          break;
        }
      }
    }
    sleep(1);
  }

  if (!detected) {
    return 6;
  }

  // wait act nfc dep
  while (1) {
    err = rfalNfcDepListenGetActivationStatus();
    if (err == ERR_BUSY) {
      printf("device is busy.\n");
    } else if (err != ERR_NONE) {
      printf("failed to get activation status: %d\n", err);
      return 7;
    } else {
      printf("activation status is ok.\n");
      break;
    }
    sleep(1);
  }

  // TODO: data exchange
  while (1) {
    rfalNfcDepTxRxParam rfalNfcDepTxRx;

    err = rfalNfcDepGetTransceiveStatus();
    if (err == ERR_BUSY) {
      printf("device is busy.\n");
    } else if (err != ERR_NONE) {
      printf("failed to get transceive status: %d\n", err);
      return 8;
    } else {
      printf("transceive status is ok.\n");
      printf("received: ");
      for (int i = 0; i < gRxLen; i++) {
        printf("%02X ", gRxBuf.nfcDepRxBuf.inf[i]);
      }
      printf("\n");

      /* Loop/Send back the same data that has been received */
      rfalNfcDepTxRx.txBuf = &gRxBuf.nfcDepRxBuf;
      rfalNfcDepTxRx.txBufLen = gRxLen;
      rfalNfcDepTxRx.rxBuf = &gRxBuf.nfcDepRxBuf;
      rfalNfcDepTxRx.rxLen = &gRxLen;
      rfalNfcDepTxRx.DID = RFAL_NFCDEP_DID_NO;
      rfalNfcDepTxRx.FSx = rfalNfcDepLR2FS(
          rfalNfcDepPP2LR(gNfcDepDev.activation.Initiator.ATR_REQ.PPi));
      rfalNfcDepTxRx.FWT = gNfcDepDev.info.FWT;
      rfalNfcDepTxRx.dFWT = gNfcDepDev.info.dFWT;
      rfalNfcDepTxRx.isRxChaining = &gIsRxChaining;
      rfalNfcDepTxRx.isTxChaining = gIsRxChaining;

      err = rfalNfcDepStartTransceive(&rfalNfcDepTxRx);
      if (err != ERR_NONE) {
        printf("failed to start transceive: %d\n", err);
      } else {
        break;
      }
    }
    sleep(1);
  }

  rfalListenStop();
  printf("done\n");

  return 0;
}
