/**
 ******************************************************************************
 *
 * COPYRIGHT(c) 2017 STMicroelectronics
 *
 * Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *notice, this list of conditions and the following disclaimer in the
 *documentation and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

/*! \file
 *
 *  \author
 *
 *  \brief Demo application
 *
 *  This demo shows how to poll for several types of NFC cards/devices and how
 *  to exchange data with these devices, using the RFAL library.
 *
 *  This demo does not fully implement the activities according to the
 * standards, it performs the required to communicate with a card/device and
 * retrieve its UID. Also blocking methods are used for data exchange which may
 * lead to long periods of blocking CPU/MCU. For standard compliant example
 * please refer to the Examples provided with the RFAL library.
 *
 */

/*
 ******************************************************************************
 * INCLUDES
 ******************************************************************************
 */
#include <stdio.h>
#include "rfal_isoDep.h"
#include "rfal_nfcDep.h"
#include "rfal_nfca.h"
#include "rfal_nfcb.h"
#include "rfal_nfcf.h"
#include "rfal_nfcv.h"
#include "rfal_rf.h"
#include "rfal_st25tb.h"
#include "utils.h"

/*
******************************************************************************
* GLOBAL DEFINES
******************************************************************************
*/

/* Definition of possible states the demo state machine could have */
#define DEMO_ST_FIELD_OFF 0
#define DEMO_ST_POLL_ACTIVE_TECH 1
#define DEMO_ST_POLL_PASSIV_TECH 2
#define DEMO_ST_WAIT_WAKEUP 3
#define DEMO_ST_LISTEN_INI 4
#define DEMO_ST_LISTEN_WAIT_ACT_RF 5
#define DEMO_ST_LISTEN_WAIT_ACT_NFCDEP 6
#define DEMO_ST_LISTEN_DATAEXCHANGE 7

#define DEMO_BUF_LEN 255

#define DEMO_LISTEN_DUR 3000

/* macro to cycle through states */
#define NEXT_STATE()                                                           \
  {                                                                            \
    state++;                                                                   \
    state %= sizeof(stateArray);                                               \
  }

/*
 ******************************************************************************
 * LOCAL VARIABLES
 ******************************************************************************
 */

/* State array of all possible states to be executed one after each other */
static uint8_t stateArray[] = {
    DEMO_ST_FIELD_OFF,
    DEMO_ST_POLL_ACTIVE_TECH,
    DEMO_ST_POLL_PASSIV_TECH,
    DEMO_ST_WAIT_WAKEUP,
    DEMO_ST_LISTEN_INI,
    DEMO_ST_LISTEN_WAIT_ACT_RF,
    DEMO_ST_LISTEN_WAIT_ACT_NFCDEP,
    DEMO_ST_LISTEN_DATAEXCHANGE,
};

/* P2P communication data */
static uint8_t NFCID3[] = {0x01, 0xFE, 0x03, 0x04, 0x05,
                           0x06, 0x07, 0x08, 0x09, 0x0A};
static uint8_t GB[] = {0x46, 0x66, 0x6d, 0x01, 0x01, 0x11, 0x02,
                       0x02, 0x07, 0x80, 0x03, 0x02, 0x00, 0x03,
                       0x04, 0x01, 0x32, 0x07, 0x01, 0x03};


/* APDUs communication data */
static uint8_t ndefSelectApp[] = {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76,
                                  0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
static uint8_t ccSelectFile[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03};
static uint8_t readBynary[] = {0x00, 0xB0, 0x00, 0x00, 0x0F};
/*static uint8_t ppseSelectApp[] = { 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50,
 * 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00
 * };*/

/* P2P communication data */
static uint8_t ndefPing[] = {0x00, 0x00};
static uint8_t ndefInit[] = {0x05, 0x20, 0x06, 0x0F, 0x75, 0x72, 0x6E,
                             0x3A, 0x6E, 0x66, 0x63, 0x3A, 0x73, 0x6E,
                             0x3A, 0x73, 0x6E, 0x65, 0x70, 0x02, 0x02,
                             0x07, 0x80, 0x05, 0x01, 0x02};
static uint8_t ndefUriSTcom[] = {
    0x13, 0x20, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x19, 0xc1, 0x01, 0x00,
    0x00, 0x00, 0x12, 0x55, 0x00, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
    0x77, 0x77, 0x77, 0x2e, 0x73, 0x74, 0x2e, 0x63, 0x6f, 0x6d};

/*
 ******************************************************************************
 * LOCAL VARIABLES
 ******************************************************************************
 */

/*! Transmit buffers union, only one interface is used at a time */
static union {
  rfalIsoDepApduBufFormat
      isoDepTxBuf; /* ISO-DEP Tx buffer format (with header/prologue) */
  rfalNfcDepBufFormat
      nfcDepTxBuf; /* NFC-DEP Rx buffer format (with header/prologue) */
  uint8_t txBuf[DEMO_BUF_LEN]; /* Generic buffer abstraction */
} gTxBuf;

/*! Receive buffers union, only one interface is used at a time */
static union {
  rfalIsoDepApduBufFormat
      isoDepRxBuf; /* ISO-DEP Rx buffer format (with header/prologue) */
  rfalNfcDepBufFormat
      nfcDepRxBuf; /* NFC-DEP Rx buffer format (with header/prologue) */
  uint8_t rxBuf[DEMO_BUF_LEN]; /* Generic buffer abstraction */
} gRxBuf;

static rfalIsoDepBufFormat tmpBuf; /* tmp buffer required for ISO-DEP APDU
                                      interface, I-Block interface does not */

/*! Receive buffers union, only one interface is used at a time */
static union {
  rfalIsoDepDevice isoDepDev; /* ISO-DEP Device details */
  rfalNfcDepDevice nfcDepDev; /* NFC-DEP Device details */
} gDevProto;

static bool doWakeUp = false; /*!< by default do not perform Wake-Up */
static uint8_t state =
    DEMO_ST_FIELD_OFF; /*!< Actual state, starting with RF field turned off */

uint16_t gRxLen;
uint32_t gTick;
bool gIsRxChaining;          /*!< Received data is not complete   */
rfalNfcDepDevice gNfcDepDev; /*!< NFC-DEP device info             */

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/

static bool demoPollAP2P(void);
static bool demoListen(void);
// static bool demoPollNFCA( void );
// static bool demoPollNFCB( void );
// static bool demoPollST25TB( void );
// static bool demoPollNFCF( void );
// static bool demoPollNFCV( void );
static ReturnCode demoActivateP2P(uint8_t *nfcid, uint8_t nfidLen,
                                  bool isActive, rfalNfcDepDevice *nfcDepDev);
static ReturnCode demoNfcDepBlockingTxRx(rfalNfcDepDevice *nfcDepDev,
                                         const uint8_t *txBuf,
                                         uint16_t txBufSize, uint8_t *rxBuf,
                                         uint16_t rxBufSize,
                                         uint16_t *rxActLen);
static ReturnCode demoIsoDepBlockingTxRx(rfalIsoDepDevice *isoDepDev,
                                         const uint8_t *txBuf,
                                         uint16_t txBufSize, uint8_t *rxBuf,
                                         uint16_t rxBufSize,
                                         uint16_t *rxActLen);
static void demoSendNdefUri(void);
static void demoSendAPDUs(void);

char* hex2Str(uint8_t* bytes, int len) {
  char str[256];
  for (int i = 0; i < len; i++) {
    sprintf(str+i*2, "%02X", bytes[i]);
  }
  return str;
}

/*!
 *****************************************************************************
 * \brief Demo Cycle
 *
 *  This function executes the actual state of the demo state machine.
 *  Must be called cyclically
 *****************************************************************************
 */
void demoCycle(void) {
  bool found = false;

  switch (stateArray[state]) {
  case DEMO_ST_FIELD_OFF:

    rfalFieldOff();
    rfalWakeUpModeStop();
    platformDelay(300);

    /* If WakeUp is to be executed, enable Wake-Up mode */
    if (doWakeUp) {
      printf("Going to Wakeup mode.\r\n");

      rfalWakeUpModeStart(NULL);
      state = DEMO_ST_WAIT_WAKEUP;
      break;
    }

    NEXT_STATE();
    break;

  case DEMO_ST_POLL_ACTIVE_TECH:
    demoPollAP2P();
    platformDelay(40);
    NEXT_STATE();
    break;

  case DEMO_ST_POLL_PASSIV_TECH:
    // found |= demoPollNFCA();
    // found |= demoPollNFCB();
    // found |= demoPollNFCF();
    // found |= demoPollNFCV();
    // found |= demoPollST25TB();

    state = ((found) ? DEMO_ST_FIELD_OFF : DEMO_ST_LISTEN_INI);
    break;

  case DEMO_ST_WAIT_WAKEUP:

    /* Check if Wake-Up Mode has been awaked */
    if (rfalWakeUpModeHasWoke()) {
      /* If awake, go directly to Poll */
      rfalWakeUpModeStop();
      state = DEMO_ST_POLL_ACTIVE_TECH;
    }
    break;

  case DEMO_ST_LISTEN_INI:
  case DEMO_ST_LISTEN_WAIT_ACT_RF:
  case DEMO_ST_LISTEN_WAIT_ACT_NFCDEP:
  case DEMO_ST_LISTEN_DATAEXCHANGE:

    demoListen();
    break;

  default:
    break;
  }
}

static bool demoListen(void) {
  ReturnCode ret;
  bool dataFlag;
  rfalLmState lmSt;
  rfalBitRate bitRate;
  uint8_t hdrLen = RFAL_NFCDEP_LEN_LEN;

  switch (state) {
  /*******************************************************************************/
  case DEMO_ST_LISTEN_INI: /* Set RF layer to perform Listen mode */

    rfalFieldOff();
    gTick = platformGetSysTick();
    ret = rfalListenStart(RFAL_LM_MASK_ACTIVE_P2P, NULL, NULL, NULL,
                          gRxBuf.rxBuf, DEMO_BUF_LEN, &gRxLen);

    if (ret != ERR_NONE) {
      break;
    }
    state = DEMO_ST_LISTEN_WAIT_ACT_RF; /* Wait for Activation from other device
                                           Reader/Initiator */
    return true;

  /*******************************************************************************/
  case DEMO_ST_LISTEN_WAIT_ACT_RF: /* Wait for Activation from other device
                                      Reader/Initiator */

    if ((platformGetSysTick() - gTick) > DEMO_LISTEN_DUR) {
      break; /* If no device is present restart loop */
    }

    lmSt = rfalListenGetState(
        &dataFlag, &bitRate); /* Check if Initator has sent some data */
    printf("state: %d, flag: %d, rate: %d\n", lmSt, dataFlag, bitRate);
    if ((lmSt == RFAL_LM_STATE_IDLE) && dataFlag) {
      /* SB Byte only in NFC-A */
      if (bitRate == RFAL_BR_106) {
        hdrLen += RFAL_NFCDEP_SB_LEN;
      }

      if (rfalNfcDepIsAtrReq(&gRxBuf.rxBuf[hdrLen],
                             (rfalConvBitsToBytes(gRxLen) - hdrLen), NULL)) {
        rfalNfcDepTargetParam param;
        rfalNfcDepListenActvParam rxParam;

        rfalListenSetState((RFAL_BR_106 == bitRate) ? RFAL_LM_STATE_TARGET_A
                                                    : RFAL_LM_STATE_TARGET_F);
        rfalSetMode(RFAL_MODE_LISTEN_ACTIVE_P2P, bitRate, bitRate);

        printf(" Activated as AP2P listener device \r\n");

        ST_MEMCPY(param.nfcid3, NFCID3, RFAL_NFCDEP_NFCID3_LEN);
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

        /* ATR_REQ received, trigger NFC-DEP layer to handle activation (sends
         * ATR_RES and handles PSL_REQ)  */
        ret = rfalNfcDepListenStartActivation(
            &param, &gRxBuf.rxBuf[hdrLen],
            (rfalConvBitsToBytes(gRxLen) - hdrLen), rxParam);
        if (ret != ERR_NONE) {
          break;
        }
        state = DEMO_ST_LISTEN_WAIT_ACT_NFCDEP;
      }
    }
    return true;

  /*******************************************************************************/
  case DEMO_ST_LISTEN_WAIT_ACT_NFCDEP: /* Wait for NFC-DEP Activation to be
                                          completed */

    ret = rfalNfcDepListenGetActivationStatus();
    if (ret != ERR_BUSY) {
      if (ret != ERR_NONE) {
        break;
      }
      state = DEMO_ST_LISTEN_DATAEXCHANGE;
    }
    return true;

  /*******************************************************************************/
  case DEMO_ST_LISTEN_DATAEXCHANGE: /* Perform Data Exchange */

    ret = rfalNfcDepGetTransceiveStatus();
    if (ret != ERR_BUSY) /* Check whether the Transceive has finished */
    {
      rfalNfcDepTxRxParam rfalNfcDepTxRx;

      if (ret != ERR_NONE) /* Check if there was any error */
      {
        printf("rfalNfcDepGetTransceiveStatus returns %d\r\n", ret);
        break;
      }

      printf(" Received %d bytes of data: %s\r\n", gRxLen,
                  hex2Str((uint8_t *)gRxBuf.nfcDepRxBuf.inf, gRxLen));

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

      ret = rfalNfcDepStartTransceive(&rfalNfcDepTxRx);
    }
    return true;
  }

  rfalListenStop();          /* Disable Listen mode */
  state = DEMO_ST_FIELD_OFF; /* Restart loop */
  return false;
}

/*!
 *****************************************************************************
 * \brief Poll NFC-AP2P
 *
 * Configures the RFAL to AP2P communication and polls for a nearby
 * device. If a device is found turns On a LED and logs its UID.
 * If the Device supports NFC-DEP protocol (P2P) it will activate
 * the device and try to send an URI record.
 *
 * This methid first tries to establish communication at 424kb/s and if
 * failed, tries also at 106kb/s
 *
 *
 *  \return true    : AP2P device found
 *  \return false   : No device found
 *
 *****************************************************************************
 */
bool demoPollAP2P(void) {
  ReturnCode err;
  bool try106 = false;

  while (!try106) {
    /*******************************************************************************/
    /* NFC_ACTIVE_POLL_MODE */
    /*******************************************************************************/
    /* Initialize RFAL as AP2P Initiator NFC BR 424 */
    err = rfalSetMode(RFAL_MODE_POLL_ACTIVE_P2P,
                      ((try106) ? RFAL_BR_106 : RFAL_BR_424),
                      ((try106) ? RFAL_BR_106 : RFAL_BR_424));

    if (err != ERR_NONE) {
      return false;
    }

    rfalSetErrorHandling(RFAL_ERRORHANDLING_NFC);
    rfalSetFDTListen(RFAL_FDT_LISTEN_AP2P_POLLER);
    rfalSetFDTPoll(RFAL_TIMING_NONE);

    rfalSetGT(RFAL_GT_AP2P_ADJUSTED);
    err = rfalFieldOnAndStartGT();

    err = demoActivateP2P(NFCID3, RFAL_NFCDEP_NFCID3_LEN, true,
                          &gDevProto.nfcDepDev);
    printf("activate: %d\n", err);
    if (err == ERR_NONE) {
      /****************************************************************************/
      /* Active P2P device activated */
      /* NFCID / UID is contained in :
       * nfcDepDev.activation.Target.ATR_RES.NFCID3 */
      printf("NFC Active P2P device found. NFCID3: %s\r\n",
                  hex2Str(gDevProto.nfcDepDev.activation.Target.ATR_RES.NFCID3,
                          RFAL_NFCDEP_NFCID3_LEN));

      /* Send an URI record */
      demoSendNdefUri();
      return true;
    }

    /* AP2P at 424kb/s didn't found any device, try at 106kb/s */
    try106 = true;
    rfalFieldOff();
  }
  printf("poll done.\n");

  return false;
}

/*!
 *****************************************************************************
 * \brief Poll NFC-A
 *
 * Configures the RFAL to NFC-A (ISO14443A) communication and polls for a nearby
 * NFC-A device.
 * If a device is found turns On a LED and logs its UID.
 *
 * Additionally, if the Device supports NFC-DEP protocol (P2P) it will activate
 * the device and try to send an URI record.
 * If the device supports ISO-DEP protocol (ISO144443-4) it will
 * activate the device and try exchange some APDUs with PICC.
 *
 *
 *  \return true    : NFC-A device found
 *  \return false   : No device found
 *
 *****************************************************************************
 */
bool demoPollNFCA(void) {
  ReturnCode err;
  bool found = false;
  uint8_t devIt = 0;
  rfalNfcaSensRes sensRes;

  rfalNfcaPollerInitialize(); /* Initialize for NFC-A */
  rfalFieldOnAndStartGT();    /* Turns the Field On if not already and start GT
                                 timer */

  err = rfalNfcaPollerTechnologyDetection(RFAL_COMPLIANCE_MODE_NFC, &sensRes);
  if (err == ERR_NONE) {
    rfalNfcaListenDevice nfcaDevList[1];
    uint8_t devCnt;

    err = rfalNfcaPollerFullCollisionResolution(RFAL_COMPLIANCE_MODE_NFC, 1,
                                                nfcaDevList, &devCnt);

    if ((err == ERR_NONE) && (devCnt > 0)) {
      found = true;
      devIt = 0;

      /* Check if it is Topaz aka T1T */
      if (nfcaDevList[devIt].type == RFAL_NFCA_T1T) {
        /********************************************/
        /* NFC-A T1T card found                     */
        /* NFCID/UID is contained in: t1tRidRes.uid */
        printf("ISO14443A/Topaz (NFC-A T1T) TAG found. UID: %s\r\n",
                    hex2Str(nfcaDevList[devIt].ridRes.uid, RFAL_T1T_UID_LEN));
      } else {
        /*********************************************/
        /* NFC-A device found                        */
        /* NFCID/UID is contained in: nfcaDev.nfcId1 */
        printf("ISO14443A/NFC-A card found. UID: %s\r\n",
                    hex2Str(nfcaDevList[0].nfcId1, nfcaDevList[0].nfcId1Len));
      }

      /* Check if device supports P2P/NFC-DEP */
      if ((nfcaDevList[devIt].type == RFAL_NFCA_NFCDEP) ||
          (nfcaDevList[devIt].type == RFAL_NFCA_T4T_NFCDEP)) {
        /* Continue with P2P Activation .... */

        err = demoActivateP2P(NFCID3, RFAL_NFCDEP_NFCID3_LEN, false,
                              &gDevProto.nfcDepDev);
        if (err == ERR_NONE) {
          /*********************************************/
          /* Passive P2P device activated              */
          printf(
              "NFCA Passive P2P device found. NFCID: %s\r\n",
              hex2Str(gDevProto.nfcDepDev.activation.Target.ATR_RES.NFCID3,
                      RFAL_NFCDEP_NFCID3_LEN));

          /* Send an URI record */
          demoSendNdefUri();
        }
      }
      /* Check if device supports ISO14443-4/ISO-DEP */
      else if (nfcaDevList[devIt].type == RFAL_NFCA_T4T) {
        /* Activate the ISO14443-4 / ISO-DEP layer */

        rfalIsoDepInitialize();
        err = rfalIsoDepPollAHandleActivation(
            (rfalIsoDepFSxI)RFAL_ISODEP_FSDI_DEFAULT, RFAL_ISODEP_NO_DID,
            RFAL_BR_424, &gDevProto.isoDepDev);
        if (err == ERR_NONE) {
          printf("ISO14443-4/ISO-DEP layer activated. \r\n");

          /* Exchange APDUs */
          demoSendAPDUs();
        }
      }
    }
  }
  return found;
}

/*!
 *****************************************************************************
 * \brief Poll NFC-B
 *
 * Configures the RFAL to NFC-B (ISO14443B) communication and polls for a nearby
 * NFC-B device.
 * If a device is found turns On a LED and logs its UID.
 * Additionally, if the Device supports ISO-DEP protocol (ISO144443-4) it will
 * activate the device and try exchange some APDUs with PICC
 *
 *  \return true    : NFC-B device found
 *  \return false   : No device found
 *
 *****************************************************************************
 */
bool demoPollNFCB(void) {
  ReturnCode err;
  rfalNfcbListenDevice nfcbDev;
  bool found = false;
  uint8_t devCnt = 0;

  /*******************************************************************************/
  /* ISO14443B/NFC_B_PASSIVE_POLL_MODE */
  /*******************************************************************************/

  rfalNfcbPollerInitialize(); /* Initialize for NFC-B */
  rfalFieldOnAndStartGT();    /* Turns the Field On if not already and start GT
                                 timer */

  err = rfalNfcbPollerCollisionResolution(RFAL_COMPLIANCE_MODE_NFC, 1, &nfcbDev,
                                          &devCnt);
  if ((err == ERR_NONE) && (devCnt > 0)) {
    /**********************************************/
    /* NFC-B card found                           */
    /* NFCID/UID is contained in: sensbRes.nfcid0 */
    found = true;
    printf("ISO14443B/NFC-B card found. UID: %s\r\n",
                hex2Str(nfcbDev.sensbRes.nfcid0, RFAL_NFCB_NFCID0_LEN));
  }

  /* Check if device supports ISO14443-4/ISO-DEP */
  if (nfcbDev.sensbRes.protInfo.FsciProType &
      RFAL_NFCB_SENSB_RES_PROTO_ISO_MASK) {

    /* Activate the ISO14443-4 / ISO-DEP layer */
    rfalIsoDepInitialize();
    err = rfalIsoDepPollBHandleActivation(
        (rfalIsoDepFSxI)RFAL_ISODEP_FSDI_DEFAULT, RFAL_ISODEP_NO_DID,
        RFAL_BR_424, RFAL_ISODEP_ATTRIB_REQ_PARAM1_DEFAULT, &nfcbDev, NULL, 0,
        &gDevProto.isoDepDev);

    if (err == ERR_NONE) {
      printf("ISO14443-4/ISO-DEP layer activated. \r\n");

      /* Exchange APDUs */
      demoSendAPDUs();
    }
  }
  return found;
}

/*!
 *****************************************************************************
 * \brief Poll ST25TB
 *
 * Configures the RFAL and polls for a nearby ST25TB device.
 * If a device is found turns On a LED and logs its UID.
 *
 *  \return true    : ST25TB device found
 *  \return false   : No device found
 *
 *****************************************************************************
 */
bool demoPollST25TB(void) {
  ReturnCode err;
  bool found = false;
  uint8_t devCnt = 0;
  rfalSt25tbListenDevice st25tbDev;

  /*******************************************************************************/
  /* ST25TB_PASSIVE_POLL_MODE */
  /*******************************************************************************/

  rfalSt25tbPollerInitialize();
  rfalFieldOnAndStartGT();

  err = rfalSt25tbPollerCheckPresence(NULL);
  if (err == ERR_NONE) {
    err = rfalSt25tbPollerCollisionResolution(1, &st25tbDev, &devCnt);

    if ((err == ERR_NONE) && (devCnt > 0)) {
      /******************************************************/
      /* ST25TB card found                                  */
      /* NFCID/UID is contained in: st25tbDev.UID           */
      found = true;
      printf("ST25TB card found. UID: %s\r\n",
                  hex2Str(st25tbDev.UID, RFAL_ST25TB_UID_LEN));
    }
  }
  return found;
}

/*!
 *****************************************************************************
 * \brief Poll NFC-F
 *
 * Configures the RFAL to NFC-F (FeliCa) communication and polls for a nearby
 * NFC-F device.
 * If a device is found turns On a LED and logs its UID.
 * Additionally, if the Device supports NFC-DEP protocol (P2P) it will
 * activate the device and try to send an URI record
 *
 *  \return true    : NFC-F device found
 *  \return false   : No device found
 *
 *****************************************************************************
 */
bool demoPollNFCF(void) {
  ReturnCode err;
  rfalNfcfListenDevice nfcfDev;
  uint8_t devCnt = 0;
  bool found = false;

  /*******************************************************************************/
  /* Felica/NFC_F_PASSIVE_POLL_MODE */
  /*******************************************************************************/

  rfalNfcfPollerInitialize(RFAL_BR_212); /* Initialize for NFC-F */
  rfalFieldOnAndStartGT(); /* Turns the Field On if not already and start GT
                              timer */

  err = rfalNfcfPollerCheckPresence();
  if (err == ERR_NONE) {
    err = rfalNfcfPollerCollisionResolution(RFAL_COMPLIANCE_MODE_NFC, 1,
                                            &nfcfDev, &devCnt);

    if ((err == ERR_NONE) && (devCnt > 0)) {
      /******************************************************/
      /* NFC-F card found                                   */
      /* NFCID/UID is contained in: nfcfDev.sensfRes.NFCID2 */
      found = true;
      printf("Felica/NFC-F card found. UID: %s\r\n",
                  hex2Str(nfcfDev.sensfRes.NFCID2, RFAL_NFCF_NFCID2_LEN));

      /* Check if device supports P2P/NFC-DEP */
      if (rfalNfcfIsNfcDepSupported(&nfcfDev)) {
        /* Continue with P2P (NFC-DEP) activation */
        err = demoActivateP2P(nfcfDev.sensfRes.NFCID2, RFAL_NFCDEP_NFCID3_LEN,
                              false, &gDevProto.nfcDepDev);
        if (err == ERR_NONE) {
          /*********************************************/
          /* Passive P2P device activated              */
          printf(
              "NFCF Passive P2P device found. NFCID: %s\r\n",
              hex2Str(gDevProto.nfcDepDev.activation.Target.ATR_RES.NFCID3,
                      RFAL_NFCDEP_NFCID3_LEN));

          /* Send an URI record */
          demoSendNdefUri();
        }
      }
    }
  }
  return found;
}

/*!
 *****************************************************************************
 * \brief Poll NFC-V
 *
 * Configures the RFAL to NFC-V (ISO15693) communication, polls for a nearby
 * NFC-V device. If a device is found turns On a LED and logs its UID
 *
 *
 *  \return true    : NFC-V device found
 *  \return false   : No device found
 *
 *****************************************************************************
 */
bool demoPollNFCV(void) {
  ReturnCode err;
  rfalNfcvListenDevice nfcvDev;
  bool found = false;
  uint8_t devCnt = 0;

  /*******************************************************************************/
  /* ISO15693/NFC_V_PASSIVE_POLL_MODE */
  /*******************************************************************************/

  rfalNfcvPollerInitialize(); /* Initialize for NFC-F */
  rfalFieldOnAndStartGT();    /* Turns the Field On if not already and start GT
                                 timer */

  err = rfalNfcvPollerCollisionResolution(1, &nfcvDev, &devCnt);
  if ((err == ERR_NONE) && (devCnt > 0)) {
    /******************************************************/
    /* NFC-V card found                                   */
    /* NFCID/UID is contained in: invRes.UID */
    REVERSE_BYTES(nfcvDev.InvRes.UID, RFAL_NFCV_UID_LEN);

    found = true;
    printf("ISO15693/NFC-V card found. UID: %s\r\n",
                hex2Str(nfcvDev.InvRes.UID, RFAL_NFCV_UID_LEN));
  }

  return found;
}

/*!
 *****************************************************************************
 * \brief Activate P2P
 *
 * Configures NFC-DEP layer and executes the NFC-DEP/P2P activation (ATR_REQ
 * and PSL_REQ if applicable)
 *
 * \param[in] nfcid      : nfcid to be used
 * \param[in] nfcidLen   : length of nfcid
 * \param[in] isActive   : Active or Passive communiccation
 * \param[out] nfcDepDev : If activation successful, device's Info
 *
 *  \return ERR_PARAM    : Invalid parameters
 *  \return ERR_TIMEOUT  : Timeout error
 *  \return ERR_FRAMING  : Framing error detected
 *  \return ERR_PROTO    : Protocol error detected
 *  \return ERR_NONE     : No error, activation successful
 *
 *****************************************************************************
 */
ReturnCode demoActivateP2P(uint8_t *nfcid, uint8_t nfidLen, bool isActive,
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
  return rfalNfcDepInitiatorHandleActivation(&nfcDepParams, RFAL_BR_424,
                                             nfcDepDev);
}

/*!
 *****************************************************************************
 * \brief Send URI
 *
 * Sends a NDEF URI record 'http://www.ST.com' via NFC-DEP (P2P) protocol.
 *
 * This method sends a set of static predefined frames which tries to establish
 * a LLCP connection, followed by the NDEF record, and then keeps sending
 * LLCP SYMM packets to maintain the connection.
 *
 *
 *  \return true    : NDEF URI was sent
 *  \return false   : Exchange failed
 *
 *****************************************************************************
 */
void demoSendNdefUri(void) {
  uint16_t actLen = 0;
  ReturnCode err = ERR_NONE;

  printf(" Initalize device .. ");
  if (ERR_NONE != demoNfcDepBlockingTxRx(&gDevProto.nfcDepDev, ndefInit,
                                         sizeof(ndefInit), gRxBuf.rxBuf,
                                         sizeof(gRxBuf.rxBuf), &actLen)) {
    printf("failed.");
    return;
  }
  printf("succeeded.\r\n");

  actLen = 0;
  printf(" Push NDEF Uri: www.ST.com .. ");
  if (ERR_NONE != demoNfcDepBlockingTxRx(&gDevProto.nfcDepDev, ndefUriSTcom,
                                         sizeof(ndefUriSTcom), gRxBuf.rxBuf,
                                         sizeof(gRxBuf.rxBuf), &actLen)) {
    printf("failed.");
    return;
  }
  printf("succeeded.\r\n");

  printf(" Device present, maintaining connection ");
  while (err == ERR_NONE) {
    err =
        demoNfcDepBlockingTxRx(&gDevProto.nfcDepDev, ndefPing, sizeof(ndefPing),
                               gRxBuf.rxBuf, sizeof(gRxBuf.rxBuf), &actLen);
    printf(".");
    platformDelay(50);
  }
  printf("\r\n Device removed.\r\n");
}

/*!
 *****************************************************************************
 * \brief Exchange APDUs
 *
 * Example how to exchange a set of predefined APDUs with PICC. The NDEF
 * application will be selected and then CC will be selected and read.
 *
 *****************************************************************************
 */
void demoSendAPDUs(void) {
  uint16_t rxLen;
  ReturnCode err;

  /* Exchange APDU: NDEF Tag Application Select command */
  err = demoIsoDepBlockingTxRx(&gDevProto.isoDepDev, ndefSelectApp,
                               sizeof(ndefSelectApp), gRxBuf.rxBuf,
                               sizeof(gRxBuf.rxBuf), &rxLen);

  if ((err == ERR_NONE) && gRxBuf.rxBuf[0] == 0x90 && gRxBuf.rxBuf[1] == 0x00) {
    printf(" Select NDEF App successfully \r\n");

    /* Exchange APDU: Select Capability Container File */
    err = demoIsoDepBlockingTxRx(&gDevProto.isoDepDev, ccSelectFile,
                                 sizeof(ccSelectFile), gRxBuf.rxBuf,
                                 sizeof(gRxBuf.rxBuf), &rxLen);

    /* Exchange APDU: Read Capability Container File  */
    err = demoIsoDepBlockingTxRx(&gDevProto.isoDepDev, readBynary,
                                 sizeof(readBynary), gRxBuf.rxBuf,
                                 sizeof(gRxBuf.rxBuf), &rxLen);
  }
}

/*!
 *****************************************************************************
 * \brief ISO-DEP Blocking Transceive
 *
 * Helper function to send data in a blocking manner via the rfalIsoDep module
 *
 * \warning A protocol transceive handles long timeouts (several seconds),
 * transmission errors and retransmissions which may lead to a long period of
 * time where the MCU/CPU is blocked in this method.
 * This is a demo implementation, for a non-blocking usage example please
 * refer to the Examples available with RFAL
 *
 *
 * \param[in]  isoDepDev  : device details retrived during activation
 * \param[in]  txBuf      : data to be transmitted
 * \param[in]  txBufSize  : size of the data to be transmited
 * \param[out] rxBuf      : buffer to place receive data
 * \param[in]  rxBufSize  : size of the reception buffer
 * \param[out] rxActLen   : number of data bytes received

 *
 *  \return ERR_PARAM     : Invalid parameters
 *  \return ERR_TIMEOUT   : Timeout error
 *  \return ERR_FRAMING   : Framing error detected
 *  \return ERR_PROTO     : Protocol error detected
 *  \return ERR_NONE      : No error, activation successful
 *
 *****************************************************************************
 */
ReturnCode demoIsoDepBlockingTxRx(rfalIsoDepDevice *isoDepDev,
                                  const uint8_t *txBuf, uint16_t txBufSize,
                                  uint8_t *rxBuf, uint16_t rxBufSize,
                                  uint16_t *rxActLen) {
  ReturnCode err;
  rfalIsoDepApduTxRxParam isoDepTxRx;

  /* Initialize the ISO-DEP protocol transceive context */
  isoDepTxRx.txBuf = &gTxBuf.isoDepTxBuf;
  isoDepTxRx.txBufLen = txBufSize;
  isoDepTxRx.DID = isoDepDev->info.DID;
  isoDepTxRx.FWT = isoDepDev->info.FWT;
  isoDepTxRx.dFWT = isoDepDev->info.dFWT;
  isoDepTxRx.FSx = isoDepDev->info.FSx;
  isoDepTxRx.ourFSx = RFAL_ISODEP_FSX_KEEP;
  isoDepTxRx.rxBuf = &gRxBuf.isoDepRxBuf;
  isoDepTxRx.rxLen = rxActLen;
  isoDepTxRx.tmpBuf = &tmpBuf;

  /* Copy data to send */
  ST_MEMMOVE(gTxBuf.isoDepTxBuf.apdu, txBuf,
             MIN(txBufSize, RFAL_ISODEP_DEFAULT_FSC));

  /* Perform the ISO-DEP Transceive in a blocking way */
  rfalIsoDepStartApduTransceive(isoDepTxRx);
  do {
    rfalWorker();
    err = rfalIsoDepGetApduTransceiveStatus();
  } while (err == ERR_BUSY);

  printf(
      " ISO-DEP TxRx %s: - Tx: %s Rx: %s \r\n",
      (err != ERR_NONE) ? "FAIL" : "OK", hex2Str((uint8_t *)txBuf, txBufSize),
      (err != ERR_NONE) ? "" : hex2Str(isoDepTxRx.rxBuf->apdu, *rxActLen));

  if (err != ERR_NONE) {
    return err;
  }

  /* Copy received data */
  ST_MEMMOVE(rxBuf, isoDepTxRx.rxBuf->apdu, MIN(*rxActLen, rxBufSize));
  return ERR_NONE;
}

/*!
 *****************************************************************************
 * \brief NFC-DEP Blocking Transceive
 *
 * Helper function to send data in a blocking manner via the rfalNfcDep module
 *
 * \warning A protocol transceive handles long timeouts (several seconds),
 * transmission errors and retransmissions which may lead to a long period of
 * time where the MCU/CPU is blocked in this method.
 * This is a demo implementation, for a non-blocking usage example please
 * refer to the Examples available with RFAL
 *
 * \param[in]  nfcDepDev  : device details retrived during activation
 * \param[in]  txBuf      : data to be transmitted
 * \param[in]  txBufSize  : size of the data to be transmited
 * \param[out] rxBuf      : buffer to place receive data
 * \param[in]  rxBufSize  : size of the reception buffer
 * \param[out] rxActLen   : number of data bytes received

 *
 *  \return ERR_PARAM     : Invalid parameters
 *  \return ERR_TIMEOUT   : Timeout error
 *  \return ERR_FRAMING   : Framing error detected
 *  \return ERR_PROTO     : Protocol error detected
 *  \return ERR_NONE      : No error, activation successful
 *
 *****************************************************************************
 */
ReturnCode demoNfcDepBlockingTxRx(rfalNfcDepDevice *nfcDepDev,
                                  const uint8_t *txBuf, uint16_t txBufSize,
                                  uint8_t *rxBuf, uint16_t rxBufSize,
                                  uint16_t *rxActLen) {
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

  // printf(" NFC-DEP TxRx %s: - Tx: %s Rx: %s \r\n", (err != ERR_NONE) ?
  // "FAIL": "OK", hex2Str( (uint8_t*)rfalNfcDepTxRx.txBuf, txBufSize), (err !=
  // ERR_NONE) ? "": hex2Str( rfalNfcDepTxRx.rxBuf->inf, *rxActLen));

  if (err != ERR_NONE) {
    return err;
  }

  /* Copy received data */
  ST_MEMMOVE(rxBuf, gRxBuf.nfcDepRxBuf.inf, MIN(*rxActLen, rxBufSize));
  return ERR_NONE;
}

int main(void) {
  ReturnCode err;

  if (gpio_init() != ERR_NONE) {
    printf("h\n");
    return 1;
  }
  if (spi_init() != ERR_NONE) {
    printf("b\n");
    return 2;
  }
  if (interrupt_init() != ERR_NONE) {
    printf("c\n");
    return 3;
  }

  while (1) {
    demoCycle();
  }

  return 0;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
