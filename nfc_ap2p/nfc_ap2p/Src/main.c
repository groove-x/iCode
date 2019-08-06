#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rfal_analogConfig.h"
#include "rfal_nfcDep.h"
#include "utils.h"

#define BUF_LEN 255

static bool gIsRxChaining;
static uint16_t gRxLen;
static rfalNfcDepDevice gNfcDepDev;
static uint8_t NFCID3[] = {0x01, 0xFE, 0x03, 0x04, 0x05,
                           0x06, 0x07, 0x08, 0x09, 0x0A};

static uint8_t GB[] = {0x46, 0x66, 0x6d, 0x01, 0x01, 0x11, 0x02,
                       0x02, 0x07, 0x80, 0x03, 0x02, 0x00, 0x03,
                       0x04, 0x01, 0x32, 0x07, 0x01, 0x03};

static union {
  rfalNfcDepDevice nfcDepDev; /* NFC-DEP Device details */
} gDevProto;

static union {
  rfalNfcDepBufFormat nfcDepRxBuf;
  uint8_t rxBuf[BUF_LEN];
} gRxBuf;

static union {
  rfalNfcDepBufFormat nfcDepTxBuf;
  uint8_t txBuf[BUF_LEN];
} gTxBuf;

ReturnCode nfcDepBlockingTxRx(rfalNfcDepDevice *nfcDepDev, const uint8_t *txBuf,
                              uint16_t txBufSize, uint8_t *rxBuf,
                              uint16_t rxBufSize, uint16_t *rxActLen) {
  ReturnCode err;
  bool isChaining;
  rfalNfcDepTxRxParam rfalNfcDepTxRx;

  /* Initialize the NFC-DEP protocol transceive context */
  rfalNfcDepTxRx.txBuf = &gTxBuf.nfcDepTxBuf;
  rfalNfcDepTxRx.txBufLen = txBufSize;
  rfalNfcDepTxRx.rxBuf = &gRxBuf.nfcDepRxBuf;
  rfalNfcDepTxRx.rxLen = rxActLen;
  rfalNfcDepTxRx.DID = RFAL_NFCDEP_DID_NO;
  rfalNfcDepTxRx.FSx = rfalNfcDepLR2FS(
      rfalNfcDepPP2LR(nfcDepDev->activation.Target.ATR_RES.PPt));
  rfalNfcDepTxRx.FWT = nfcDepDev->info.FWT;
  rfalNfcDepTxRx.dFWT = nfcDepDev->info.dFWT;
  rfalNfcDepTxRx.isRxChaining = &isChaining;
  rfalNfcDepTxRx.isTxChaining = false;

  /* Copy data to send */
  ST_MEMMOVE(gTxBuf.nfcDepTxBuf.inf, txBuf,
             MIN(txBufSize, RFAL_NFCDEP_FRAME_SIZE_MAX_LEN));

  /* Perform the NFC-DEP Transceive in a blocking way */
  rfalNfcDepStartTransceive(&rfalNfcDepTxRx);
  do {
    rfalWorker();
    err = rfalNfcDepGetTransceiveStatus();
  } while (err == ERR_BUSY);

  if (err != ERR_NONE) {
    return err;
  }

  /* Copy received data */
  memmove(rxBuf, gRxBuf.nfcDepRxBuf.inf, MIN(*rxActLen, rxBufSize));
  return ERR_NONE;
}

int sendNdefUri(void) {
  uint16_t actLen = 0;
  ReturnCode err = ERR_NONE;

  printf("initalize device .. ");
  uint8_t ndefInit[] = {0x05, 0x20, 0x06, 0x0F, 0x75, 0x72, 0x6E, 0x3A, 0x6E,
                        0x66, 0x63, 0x3A, 0x73, 0x6E, 0x3A, 0x73, 0x6E, 0x65,
                        0x70, 0x02, 0x02, 0x07, 0x80, 0x05, 0x01, 0x02};
  if (ERR_NONE != nfcDepBlockingTxRx(&gDevProto.nfcDepDev, ndefInit,
                                     sizeof(ndefInit), gRxBuf.rxBuf,
                                     sizeof(gRxBuf.rxBuf), &actLen)) {
    printf("failed.\n");
    return 1;
  } else {
    printf("succeeded.\n");
  }

  actLen = 0;
  printf("Push NDEF Uri: www.ST.com .. ");
  uint8_t ndefUriSTcom[] = {
      0x13, 0x20, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x19, 0xc1, 0x01, 0x00,
      0x00, 0x00, 0x12, 0x55, 0x00, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
      0x77, 0x77, 0x77, 0x2e, 0x73, 0x74, 0x2e, 0x63, 0x6f, 0x6d};
  if (ERR_NONE != nfcDepBlockingTxRx(&gDevProto.nfcDepDev, ndefUriSTcom,
                                     sizeof(ndefUriSTcom), gRxBuf.rxBuf,
                                     sizeof(gRxBuf.rxBuf), &actLen)) {
    printf("failed.\n");
    return 2;
  } else {
    printf("succeeded.\n");
  }

  printf("Device present, maintaining connection ");
  uint8_t ndefPing[] = {0x00, 0x00};
  while (err == ERR_NONE) {
    err = nfcDepBlockingTxRx(&gDevProto.nfcDepDev, ndefPing, sizeof(ndefPing),
                             gRxBuf.rxBuf, sizeof(gRxBuf.rxBuf), &actLen);
    printf(".");
    platformDelay(50);
  }
  printf("Device removed.\n");

  return 0;
}

ReturnCode activateP2P(uint8_t *nfcid, uint8_t nfidLen, bool isActive,
                       rfalNfcDepDevice *nfcDepDev) {
  rfalNfcDepAtrParam nfcDepParams;

  nfcDepParams.nfcid = nfcid;
  nfcDepParams.nfcidLen = nfidLen;
  nfcDepParams.BS = RFAL_NFCDEP_Bx_NO_HIGH_BR;
  nfcDepParams.BR = RFAL_NFCDEP_Bx_NO_HIGH_BR;
  nfcDepParams.LR = RFAL_NFCDEP_LR_254;
  nfcDepParams.DID = RFAL_NFCDEP_DID_NO;
  nfcDepParams.NAD = RFAL_NFCDEP_NAD_NO;
  nfcDepParams.GBLen = sizeof(GB);
  nfcDepParams.GB = GB;
  nfcDepParams.commMode =
      ((isActive) ? RFAL_NFCDEP_COMM_ACTIVE : RFAL_NFCDEP_COMM_PASSIVE);
  nfcDepParams.operParam =
      (RFAL_NFCDEP_OPER_FULL_MI_EN | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS |
       RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);

  /* Initialize NFC-DEP protocol layer */
  rfalNfcDepInitialize();

  /* Handle NFC-DEP Activation (ATR_REQ and PSL_REQ if applicable) */
  return rfalNfcDepInitiatorHandleActivation(&nfcDepParams, RFAL_BR_424, nfcDepDev);
}

void pollAP2P() {
  ReturnCode err;

  printf("set AP2P mode\n");
  err = rfalSetMode(RFAL_MODE_POLL_ACTIVE_P2P, RFAL_BR_424, RFAL_BR_424);
  if (err != ERR_NONE) {
    printf("failed to set mode: %d\n", err);
    return;
  }

  // start GT
  rfalSetErrorHandling(RFAL_ERRORHANDLING_NFC);
  rfalSetFDTListen(RFAL_FDT_LISTEN_AP2P_POLLER);
  rfalSetFDTPoll(RFAL_TIMING_NONE);
  rfalSetGT(RFAL_GT_AP2P_ADJUSTED);
  err = rfalFieldOnAndStartGT();
  if (err != ERR_NONE) {
    printf("failed to startGT: %d\n", err);
    return;
  }

  // activate P2P
  err = activateP2P(NFCID3, RFAL_NFCDEP_NFCID3_LEN, true, &gDevProto.nfcDepDev);
  if (err != ERR_NONE) {
    // not detected device
    printf("NFC Active P2P device not found: %d\n", err);
  } else {
    // detected device
    printf("NFC Active P2P device found. NFCID3: ");
    for (int i = 0; i < RFAL_NFCDEP_NFCID3_LEN; i++) {
      printf("%02X ", gDevProto.nfcDepDev.activation.Target.ATR_RES.NFCID3[i]);
    }
    printf("\n");

    int ret = sendNdefUri();
    printf("send result: %d\n", ret);
  }
  rfalFieldOff();
}

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
  rfalAnalogConfigInitialize();
  err = rfalInitialize();
  if (err != ERR_NONE) {
    printf("failed to initialize\n");
    return 4;
  }
  printf("initialize done.\n");

  err = rfalFieldOff();
  if (err != ERR_NONE) {
    printf("failed to field off: %d\n", err);
    return 5;
  }

  rfalWakeUpModeStop();
  sleep(1);

  rfalWakeUpModeStart(NULL);

  while (!rfalWakeUpModeHasWoke()) {
    break;
  }
  rfalWakeUpModeStop();
  sleep(1);
  printf("start\n");

  // poll active p2p
  pollAP2P();
  sleep(1);

  // init listen
  rfalFieldOff();
  int tick = time(NULL);
  err = rfalListenStart(RFAL_LM_MASK_ACTIVE_P2P, NULL, NULL, NULL, gRxBuf.rxBuf, BUF_LEN, &gRxLen);
  if (err != ERR_NONE) {
    printf("failed to listen start: %d\n", err);
    return 4;
  }
  printf("listen start done.\n");

  // wait act rf
  bool detected = false;
  while (time(NULL) - tick < 30) {
    bool dataFlag = false;
    rfalBitRate bitRate;
    rfalLmState lmst = RFAL_LM_STATE_NOT_INIT;
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

  // data exchange
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