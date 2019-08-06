#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rfal_analogConfig.h"
#include "rfal_nfcDep.h"
#include "utils.h"

#define BUF_LEN 255

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
  ST_MEMMOVE(rxBuf, gRxBuf.nfcDepRxBuf.inf, MIN(*rxActLen, rxBufSize));
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
  uint8_t NFCID3[] = {0x01, 0xFE, 0x03, 0x04, 0x05,
                      0x06, 0x07, 0x08, 0x09, 0x0A};

  uint8_t GB[] = {0x46, 0x66, 0x6d, 0x01, 0x01, 0x11, 0x02, 0x02, 0x07, 0x80,
                  0x03, 0x02, 0x00, 0x03, 0x04, 0x01, 0x32, 0x07, 0x01, 0x03};
  rfalNfcDepAtrParam nfcDepParams;
  nfcDepParams.nfcid = NFCID3;
  nfcDepParams.nfcidLen = RFAL_NFCDEP_NFCID3_LEN;
  nfcDepParams.BS = RFAL_NFCDEP_Bx_NO_HIGH_BR;
  nfcDepParams.BR = RFAL_NFCDEP_Bx_NO_HIGH_BR;
  nfcDepParams.LR = RFAL_NFCDEP_LR_254;
  nfcDepParams.DID = RFAL_NFCDEP_DID_NO;
  nfcDepParams.NAD = RFAL_NFCDEP_NAD_NO;
  nfcDepParams.GBLen = sizeof(GB);
  nfcDepParams.GB = GB;
  nfcDepParams.commMode = RFAL_NFCDEP_COMM_ACTIVE;
  nfcDepParams.operParam =
      (RFAL_NFCDEP_OPER_FULL_MI_EN | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS |
       RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);

  rfalNfcDepInitialize();
  err = rfalNfcDepInitiatorHandleActivation(&nfcDepParams, RFAL_BR_424,
                                            &gDevProto.nfcDepDev);
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

  // poll active p2p
  while (1) {
    pollAP2P();
    sleep(1);
  }

  return 0;
}
