
/******************************************************************************
  *
  * Example software provided by Bostin Technology Ltd
  *
  * This software comes with no warrently and is provided as a demo application
  * for use with the ST25R3911B NFC controller
  *
  * For more information    www.Cogniot.eu
  *
  * Used in conjunction with STMicroelectronics
  *
  *        http://www.st.com/myliberty
  *
******************************************************************************/


/*
 ******************************************************************************
 * INCLUDES
 ******************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include "exampleNFC.h"
#include "logger.h"
#include "st_errno.h"
#include "utils.h"
#include "platform.h"
#include "rfal_nfca.h"
#include "rfal_nfcb.h"
#include "rfal_nfcf.h"
#include "rfal_nfcv.h"
#include "rfal_isoDep.h"
#include "rfal_nfcDep.h"
#include "rfal_analogConfig.h"

const char* LOG_HEADER = "\r\nDemo Software provided by Bostin Technology\n\rScanning for NFC technologies \n\r";



#define LOG_BUFFER_SIZE 4096
static char logBuffer[LOG_BUFFER_SIZE];
static uint32_t logCnt = 0;

static int platformLog(const char* format, ...);
static void platformLogCreateHeader(char* buf);

#define platformLogClear()              system("clear")
#define platformLog2Screen(buf)         printf(buf); platformLogCreateHeader(buf);//buf[0] = 0;



/*
******************************************************************************
* GLOBAL DEFINES
******************************************************************************
*/
#define EXAMPLE_RFAL_POLLER_DEVICES      10    /* Number of devices supported */
#define EXAMPLE_RFAL_POLLER_RF_BUF_LEN   255   /* RF buffer length            */

#define EXAMPLE_RFAL_POLLER_FOUND_NONE   0x00  /* No device found Flag        */
#define EXAMPLE_RFAL_POLLER_FOUND_A      0x01  /* NFC-A device found Flag     */
#define EXAMPLE_RFAL_POLLER_FOUND_B      0x02  /* NFC-B device found Flag     */
#define EXAMPLE_RFAL_POLLER_FOUND_F      0x04  /* NFC-F device found Flag     */
#define EXAMPLE_RFAL_POLLER_FOUND_V      0x08  /* NFC-V device Flag           */


/*
******************************************************************************
* GLOBAL TYPES
******************************************************************************
*/

/*! Main state                                                                          */
typedef enum{
    EXAMPLE_RFAL_POLLER_STATE_INIT                =  0,  /* Initialize state            */
    EXAMPLE_RFAL_POLLER_STATE_TECHDETECT          =  1,  /* Technology Detection state  */
    EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE        =  2,  /* Collision Avoidance state   */
    EXAMPLE_RFAL_POLLER_STATE_ACTIVATION          =  3,  /* Activation state            */
    EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START  =  4,  /* Data Exchange Start state   */
    EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_CHECK  =  5,  /* Data Exchange Check state   */
    EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION        =  9   /* Deactivation state          */
}exampleRfalPollerState;


/*! Device type                                                                         */
typedef enum{
    EXAMPLE_RFAL_POLLER_TYPE_NFCA  =  0,                 /* NFC-A device type           */
    EXAMPLE_RFAL_POLLER_TYPE_NFCB  =  1,                 /* NFC-B device type           */
    EXAMPLE_RFAL_POLLER_TYPE_NFCF  =  2,                 /* NFC-F device type           */
    EXAMPLE_RFAL_POLLER_TYPE_NFCV  =  3,                 /* NFC-V device type           */
    EXAMPLE_RFAL_POLLER_TYPE_NONE  =  99                 /* UNknown or not selected NFC type */
}exampleRfalPollerDevType;


/*! Device interface                                                                    */
typedef enum{
    EXAMPLE_RFAL_POLLER_INTERFACE_RF     = 0,            /* RF Frame interface          */
    EXAMPLE_RFAL_POLLER_INTERFACE_ISODEP = 1,            /* ISO-DEP interface           */
    EXAMPLE_RFAL_POLLER_INTERFACE_NFCDEP = 2             /* NFC-DEP interface           */
}exampleRfalPollerRfInterface;


/*! Device struct containing all its details                                            */
typedef struct{
    exampleRfalPollerDevType type;                      /* Device's type                */
    union{
        rfalNfcaListenDevice nfca;                      /* NFC-A Listen Device instance */
        rfalNfcbListenDevice nfcb;                      /* NFC-B Listen Device instance */
        rfalNfcfListenDevice nfcf;                      /* NFC-F Listen Device instance */
        rfalNfcvListenDevice nfcv;                      /* NFC-V Listen Device instance */
    }dev;                                               /* Device's instance            */
    
    exampleRfalPollerRfInterface rfInterface;           /* Device's interface           */
    union{
        rfalIsoDepDevice isoDep;                        /* ISO-DEP instance             */
        rfalNfcDepDevice nfcDep;                        /* NFC-DEP instance             */
    }proto;                                             /* Device's protocol            */
    
}exampleRfalPollerDevice;


/*
 ******************************************************************************
 * LOCAL VARIABLES
 ******************************************************************************
 */
static uint8_t                 t1tReadReq[]    = { 0x01, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44 };                                                   /* T1T READ Block:0 Byte:0 */
static uint8_t                 t2tReadReq[]    = { 0x30, 0x00 };                                                                                 /* T2T READ Block:0 */
static uint8_t                 t3tCheckReq[]   = { 0x06, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x01, 0x09, 0x00, 0x01, 0x80, 0x00 };   /* T3T Check/Read command */
static uint8_t                 t4tSelectReq[]  = { 0x00, 0xA4, 0x00, 0x00, 0x00 };                                                               /* T4T Select MF, DF or EF APDU  */
static uint8_t                 t5tSysInfoReq[] = { 0x02, 0x2B };                                                                                 /* NFC-V Get SYstem Information command*/
static uint8_t                 nfcbReq[]       = { 0x00 };                                                                                       /* NFC-B proprietary command */
static uint8_t                 llcpSymm[]      = { 0x00, 0x00 };                                                                                 /* LLCP SYMM command */

// Added by MB as a test to send a different command
static uint8_t                 CommandToSend[] = { 0x02, 0x20, 0x00 };

static uint8_t                 gNfcid3[]       = {0x01, 0xFE, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };                                  /* NFCID3 used for ATR_REQ */
static uint8_t                 gGenBytes[]     = { 0x46, 0x66, 0x6d, 0x01, 0x01, 0x11, 0x02, 0x02, 0x07, 0x80, 0x03, 0x02, 0x00, 0x03, 0x04, 0x01, 0x32, 0x07, 0x01, 0x03 }; /* P2P General Bytes: LCCP Connect */

/*******************************************************************************/

static uint8_t                   gDevCnt;                                 /* Number of devices found                         */
static exampleRfalPollerDevice gDevList[EXAMPLE_RFAL_POLLER_DEVICES];   /* Device List                                     */
static exampleRfalPollerState  gState;                                  /* Main state                                      */
static uint8_t                   gTechsFound;                             /* Technologies found bitmask                      */
exampleRfalPollerDevice         *gActiveDev;                             /* Active device pointer                           */
static uint16_t                  gRcvLen;                                 /* Received length                                 */
static bool                       gRxChaining;                             /* Rx chaining flag                                */

/*! Transmit buffers union, only one interface is used at a time                                                           */
static union{
    uint8_t                rfTxBuf[EXAMPLE_RFAL_POLLER_RF_BUF_LEN];    /* RF Tx buffer (not used on this demo)            */
    rfalIsoDepBufFormat     isoDepTxBuf;                                /* ISO-DEP Tx buffer format (with header/prologue) */
    rfalNfcDepBufFormat     nfcDepTxBuf;                                /* NFC-DEP Rx buffer format (with header/prologue) */
}gTxBuf;


/*! Receive buffers union, only one interface is used at a time                                                            */
static union {
    uint8_t                rfRxBuf[EXAMPLE_RFAL_POLLER_RF_BUF_LEN];    /* RF Rx buffer                                    */
    rfalIsoDepBufFormat     isoDepRxBuf;                                /* ISO-DEP Rx buffer format (with header/prologue) */
    rfalNfcDepBufFormat     nfcDepRxBuf;                                /* NFC-DEP Rx buffer format (with header/prologue) */
}gRxBuf;


/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/
static bool exampleRfalPollerTechDetetection( void );
static bool exampleRfalPollerCollResolution( void );
static bool exampleRfalPollerActivation( uint8_t devIt );
static bool exampleRfalPollerNfcDepActivate( exampleRfalPollerDevice *device );
static ReturnCode exampleRfalPollerDataExchange( void );
static bool exampleRfalPollerDeactivate( void );


/*
******************************************************************************
* INITIAL SCREEN
******************************************************************************
*/
int splashscreen(void)
{
    platformLogClear();
    printf("\n***********************************************\n");
    printf("*             Bostin Technology               *\n");
    printf("*                                             *\n");
    printf("*               iCode Reader                  *\n");
    printf("*                                             *\n");
    printf("*           Based on the ST25R3911B           *\n");
    printf("*             demo provided by ST             *\n");
    printf("*                                             *\n");
    printf("*        for more info www.cognIoT.eu         *\n");
    printf("***********************************************\n");
    return (0);
}

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/

/*!
 ******************************************************************************
 * \brief Passive Poller Run
 * 
 * This method implements the main state machine going thought all the 
 * different activities that a Reader/Poller device (PCD) needs to perform.
 * 
 * 
 ******************************************************************************
 */
extern void exampleRfalPollerRun( void )
{
    ReturnCode err;
    uint8_t    i;
    
    rfalAnalogConfigInitialize();                                                     /* Initialize RFAL's Analog Configs */
    rfalInitialize();                                                                 /* Initialize RFAL */
   
	for(;;)
	{

	    rfalWorker();                                                                 /* Execute RFAL process */

      platformDelay(20);

	    switch( gState )
	    {
	        /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_INIT:                                     
	            
	            gTechsFound = EXAMPLE_RFAL_POLLER_FOUND_NONE; 
	            gActiveDev  = NULL;
	            gDevCnt     = 0;
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_TECHDETECT;
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_TECHDETECT:
	            
	            if( !exampleRfalPollerTechDetetection() )                             /* Poll for nearby devices in different technologies */
	            {
	                gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If no device was found, restart loop */
	                break;
	            }
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE;                      /* One or more devices found, go to Collision Avoidance */
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE:
	            
	            if( !exampleRfalPollerCollResolution() )                              /* Resolve any eventual collision */
                {
                    gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If Collision Resolution was unable to retrieve any device, restart loop */
                    break;
                }
	            
	            platformLog("Device(s) found: %d \r\n", gDevCnt);
	            
	            for(i=0; i<gDevCnt; i++)
	            {
	                switch( gDevList[i].type )
	                {
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCA:
	                        platformLog( " NFC-A device UID: %s \r\n", hex2str(gDevList[i].dev.nfca.nfcId1, gDevList[i].dev.nfca.nfcId1Len) );
	                        break;
	                        
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCB:
	                        platformLog( " NFC-B device UID: %s \r\n", hex2str(gDevList[i].dev.nfcb.sensbRes.nfcid0, RFAL_NFCB_NFCID0_LEN) );
                            break;
                            
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCF:
	                        platformLog( " NFC-F device UID: %s \r\n", hex2str(gDevList[i].dev.nfcf.sensfRes.NFCID2, RFAL_NFCF_NFCID2_LEN) );
                            break;
                            
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCV:
	                        platformLog( " NFC-V device UID: %s \r\n", hex2str(gDevList[i].dev.nfcv.InvRes.UID, RFAL_NFCV_UID_LEN) );
                            break;
	                }
	            }
	            gState = EXAMPLE_RFAL_POLLER_STATE_ACTIVATION;                        /* Device(s) have been identified, go to Activation */
                break;
	        
                
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_ACTIVATION:
#if 0
	            if( !exampleRfalPollerActivation( 0 ) )                               /* Any device previous identified can be Activated, on this example will select the first on the list */
	            {
                    gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If Activation failed, restart loop */
                    break;
                }
	            
	            //gState = EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START;                /* Device has been properly activated, go to Data Exchange */
	            gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;
		    break;
#endif	            
	            
            /*******************************************************************************/
#if 0	        
            case EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START:                       
	        case EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_CHECK:
	                
	            err = exampleRfalPollerDataExchange();                                /* Perform Data Exchange, in this example a simple transfer will executed in order to do device's presence check */
                switch( err )
                {
                    case ERR_NONE:                                                    /* Data exchange successful  */
                        platformDelay(300);                                           /* Wait a bit */
                        gState = EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START;        /* Trigger new exchange with device */
                        break;
                        
                    case ERR_BUSY:                                                    /* Data exchange ongoing  */
                        gState = EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_CHECK;        /* Once triggered/started the Data Exchange only do check until is completed */
                        break;
                        
                    default:                                                          /* Data exchange not successful, card removed or other transmission error */
                        platformLog("Data exchange terminated with error: %d \r\n", err);
                        gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;              /* Restart loop */
                        break;
                }
                break;
                
#endif	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION:
#if 0	            
	            exampleRfalPollerDeactivate();                                        /* If a card has been activated, properly deactivate the device */
#endif	            
	            rfalFieldOff();                                                       /* Turn the Field Off powering down any device nearby */
	            platformDelay(2);                                                     /* Remain a certain period with field off */
	            gState = EXAMPLE_RFAL_POLLER_STATE_INIT;                              /* Restart the loop */

                platformLogClear();
                platformLog2Screen(logBuffer);

	            break;
	        
	        
            /*******************************************************************************/
	        default:
	            return;
	    }
	}
}
	

/*!
 ******************************************************************************
 * \brief Poller Technology Detection
 * 
 * This method implements the Technology Detection / Poll for different 
 * device technologies.
 * 
 * \return true         : One or more devices have been detected
 * \return false         : No device have been detected
 * 
 ******************************************************************************
 */
static bool exampleRfalPollerTechDetetection( void )
{
    ReturnCode           err;
    rfalNfcaSensRes      sensRes;
    rfalNfcbSensbRes     sensbRes;
    rfalNfcvInventoryRes invRes;
    uint8_t              sensbResLen;
    
    gTechsFound = EXAMPLE_RFAL_POLLER_FOUND_NONE;
    
    /*******************************************************************************/
    /* NFC-A Technology Detection                                                  */
    /*******************************************************************************/
    
    rfalNfcaPollerInitialize();                                                       /* Initialize RFAL for NFC-A */
    rfalFieldOnAndStartGT();                                                          /* Turns the Field On and starts GT timer */
    
    err = rfalNfcaPollerTechnologyDetection( RFAL_COMPLIANCE_MODE_NFC, &sensRes ); /* Poll for NFC-A devices */
    if( err == ERR_NONE )
    {
        gTechsFound |= EXAMPLE_RFAL_POLLER_FOUND_A;
    }
    
    
    /*******************************************************************************/
    /* NFC-B Technology Detection                                                  */
    /*******************************************************************************/
    
    rfalNfcbPollerInitialize();                                                       /* Initialize RFAL for NFC-B */
    rfalFieldOnAndStartGT();                                                          /* As field is already On only starts GT timer */
    
    err = rfalNfcbPollerTechnologyDetection( RFAL_COMPLIANCE_MODE_NFC, &sensbRes, &sensbResLen ); /* Poll for NFC-B devices */
    if( err == ERR_NONE )
    {
        gTechsFound |= EXAMPLE_RFAL_POLLER_FOUND_B;
    }
    
    
    /*******************************************************************************/
    /* NFC-F Technology Detection                                                  */
    /*******************************************************************************/
    
    rfalNfcfPollerInitialize( RFAL_BR_212 );                                          /* Initialize RFAL for NFC-F */
    rfalFieldOnAndStartGT();                                                          /* As field is already On only starts GT timer */
    
    err = rfalNfcfPollerCheckPresence();                                              /* Poll for NFC-F devices */
    if( err == ERR_NONE )
    {
        gTechsFound |= EXAMPLE_RFAL_POLLER_FOUND_F;
    }
    
    
    /*******************************************************************************/
    /* NFC-V Technology Detection                                                  */
    /*******************************************************************************/
    
    rfalNfcvPollerInitialize();                                                       /* Initialize RFAL for NFC-V */
    rfalFieldOnAndStartGT();                                                          /* As field is already On only starts GT timer */
    
    err = rfalNfcvPollerCheckPresence( &invRes );                                     /* Poll for NFC-V devices */
    if( err == ERR_NONE )
    {
        gTechsFound |= EXAMPLE_RFAL_POLLER_FOUND_V;
    }
    
    return (gTechsFound != EXAMPLE_RFAL_POLLER_FOUND_NONE);
}

/*!
 ******************************************************************************
 * \brief Poller Collision Resolution
 * 
 * This method implements the Collision Resolution on all technologies that
 * have been detected before.
 * 
 * \return true         : One or more devices identified 
 * \return false        : No device have been identified
 * 
 ******************************************************************************
 */
static bool exampleRfalPollerCollResolution( void )
{
    uint8_t    i;
    uint8_t    devCnt;
    ReturnCode err;
    
    
    /*******************************************************************************/
    /* NFC-A Collision Resolution                                                  */
    /*******************************************************************************/
    if( gTechsFound & EXAMPLE_RFAL_POLLER_FOUND_A )                                   /* If a NFC-A device was found/detected, perform Collision Resolution */
    {
        rfalNfcaListenDevice nfcaDevList[EXAMPLE_RFAL_POLLER_DEVICES];
        
        rfalNfcaPollerInitialize();
        rfalFieldOnAndStartGT();                                                      /* Ensure GT again as other technologies have also been polled */
        err = rfalNfcaPollerFullCollisionResolution( RFAL_COMPLIANCE_MODE_NFC, (EXAMPLE_RFAL_POLLER_DEVICES - gDevCnt), nfcaDevList, &devCnt );
        if( (err == ERR_NONE) && (devCnt != 0) )
        {
            for( i=0; i<devCnt; i++ )                                                 /* Copy devices found form local Nfca list into global device list */
            {
                gDevList[gDevCnt].type     = EXAMPLE_RFAL_POLLER_TYPE_NFCA;
                gDevList[gDevCnt].dev.nfca = nfcaDevList[i];
                gDevCnt++;
            }
        }
    }
    
    /*******************************************************************************/
    /* NFC-B Collision Resolution                                                  */
    /*******************************************************************************/
    if( gTechsFound & EXAMPLE_RFAL_POLLER_FOUND_B )                                   /* If a NFC-A device was found/detected, perform Collision Resolution */
    {
        rfalNfcbListenDevice nfcbDevList[EXAMPLE_RFAL_POLLER_DEVICES];
        
        rfalNfcbPollerInitialize();
        rfalFieldOnAndStartGT();                                                      /* Ensure GT again as other technologies have also been polled */
        err = rfalNfcbPollerCollisionResolution( RFAL_COMPLIANCE_MODE_NFC, (EXAMPLE_RFAL_POLLER_DEVICES - gDevCnt), nfcbDevList, &devCnt );
        if( (err == ERR_NONE) && (devCnt != 0) )
        {
            for( i=0; i<devCnt; i++ )                                                 /* Copy devices found form local Nfcb list into global device list */
            {
                gDevList[gDevCnt].type     = EXAMPLE_RFAL_POLLER_TYPE_NFCB;
                gDevList[gDevCnt].dev.nfcb = nfcbDevList[i];
                gDevCnt++;
            }
        }
    }
    
    
    /*******************************************************************************/
    /* NFC-F Collision Resolution                                                  */
    /*******************************************************************************/
    if( gTechsFound & EXAMPLE_RFAL_POLLER_FOUND_F )                                   /* If a NFC-F device was found/detected, perform Collision Resolution */
    {
        rfalNfcfListenDevice nfcfDevList[EXAMPLE_RFAL_POLLER_DEVICES];
        
        rfalNfcfPollerInitialize( RFAL_BR_212 );
        rfalFieldOnAndStartGT();                                                      /* Ensure GT again as other technologies have also been polled */
        err = rfalNfcfPollerCollisionResolution( RFAL_COMPLIANCE_MODE_NFC, (EXAMPLE_RFAL_POLLER_DEVICES - gDevCnt), nfcfDevList, &devCnt );
        if( (err == ERR_NONE) && (devCnt != 0) )
        {
            for( i=0; i<devCnt; i++ )                                                 /* Copy devices found form local Nfcf list into global device list */
            {
                gDevList[gDevCnt].type     = EXAMPLE_RFAL_POLLER_TYPE_NFCF;
                gDevList[gDevCnt].dev.nfcf = nfcfDevList[i];
                gDevCnt++;
            }
        }
    }
    
    /*******************************************************************************/
    /* NFC-V Collision Resolution                                                  */
    /*******************************************************************************/
    if( gTechsFound & EXAMPLE_RFAL_POLLER_FOUND_V )                                   /* If a NFC-F device was found/detected, perform Collision Resolution */
    {
        rfalNfcvListenDevice nfcvDevList[EXAMPLE_RFAL_POLLER_DEVICES];
        
        rfalNfcvPollerInitialize();
        rfalFieldOnAndStartGT();                                                      /* Ensure GT again as other technologies have also been polled */
        err = rfalNfcvPollerCollisionResolution( (EXAMPLE_RFAL_POLLER_DEVICES - gDevCnt), nfcvDevList, &devCnt );
        if( (err == ERR_NONE) && (devCnt != 0) )
        {
            for( i=0; i<devCnt; i++ )                                                /* Copy devices found form local Nfcf list into global device list */
            {
                gDevList[gDevCnt].type     = EXAMPLE_RFAL_POLLER_TYPE_NFCV;
                gDevList[gDevCnt].dev.nfcv = nfcvDevList[i];
                gDevCnt++;
            }
        }
    }
    
    return (gDevCnt > 0);
}


/*!
 ******************************************************************************
 * \brief Poller Activation
 * 
 * This method Activates a given device according to it's type and 
 * protocols supported
 *  
 * \param[in]  devIt : device's position on the list to be activated 
 * 
 * \return true         : Activation successful 
 * \return false        : Activation failed
 * 
 ******************************************************************************
 */
static bool exampleRfalPollerActivation( uint8_t devIt )
{
    ReturnCode           err;
    rfalNfcaSensRes      sensRes;
    rfalNfcaSelRes       selRes;
    rfalNfcbSensbRes     sensbRes;
    uint8_t              sensbResLen;
    
    if( devIt > gDevCnt )
    {
        return false;
    }
    
    switch( gDevList[devIt].type )
    {
        /*******************************************************************************/
        /* NFC-A Activation                                                            */
        /*******************************************************************************/
        case EXAMPLE_RFAL_POLLER_TYPE_NFCA:
            
            rfalNfcaPollerInitialize();
            if( gDevList[devIt].dev.nfca.isSleep )                                    /* Check if desired device is in Sleep      */
            {
                err = rfalNfcaPollerCheckPresence( RFAL_14443A_SHORTFRAME_CMD_WUPA, &sensRes ); /* Wake up all cards  */
                if( err != ERR_NONE )
                {
                    return false;
                }
                
                err = rfalNfcaPollerSelect( gDevList[devIt].dev.nfca.nfcId1, gDevList[devIt].dev.nfca.nfcId1Len, &selRes ); /* Select specific device  */
                if( err != ERR_NONE )
                {
                    return false;
                }
            }
            
            /*******************************************************************************/
            /* Perform protocol specific activation                                        */
            switch( gDevList[devIt].dev.nfca.type )
            {
                /*******************************************************************************/
                case RFAL_NFCA_T1T:
                    
                    /* No further activation needed for a T1T (RID already performed)*/
                    platformLog("NFC-A T1T device activated \r\n");                   /* NFC-A T1T device activated */
                    
                    gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_RF;
                    break;
                    
                
                /*******************************************************************************/
                case RFAL_NFCA_T2T:
                  
                    /* No specific activation needed for a T2T */    
                    platformLog("NFC-A T2T device activated \r\n");                   /* NFC-A T2T device activated */
                    
                    gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_RF;
                    break;
                
                
                /*******************************************************************************/
                case RFAL_NFCA_T4T:
                
                    /* Perform ISO-DEP (ISO14443-4) activation: RATS and PPS if supported */
                    err = rfalIsoDepPollAHandleActivation( (rfalIsoDepFSxI)RFAL_ISODEP_FSDI_DEFAULT, RFAL_ISODEP_NO_DID, RFAL_BR_424, &gDevList[devIt].proto.isoDep );
                    if( err != ERR_NONE )
                    {
                        return false;
                    }
                    
                    platformLog("NFC-A T4T (ISO-DEP) device activated \r\n");         /* NFC-A T4T device activated */
                    
                    gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_ISODEP;
                    break;
                  
                  
                /*******************************************************************************/
                case RFAL_NFCA_T4T_NFCDEP:                                              /* Device supports both T4T and NFC-DEP */
                case RFAL_NFCA_NFCDEP:                                                  /* Device supports NFC-DEP */
                  
                    /* Perform NFC-DEP (P2P) activation: ATR and PSL if supported */
                    if( !exampleRfalPollerNfcDepActivate( &gDevList[devIt] ) )
                    {
                      return false;
                    }
                    
                    platformLog("NFC-A P2P (NFC-DEP) device activated \r\n");         /* NFC-A P2P device activated */
                    gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_NFCDEP;
                    break;
            }
            
            break;
        
        /*******************************************************************************/
        /* NFC-B Activation                                                            */
        /*******************************************************************************/
        case EXAMPLE_RFAL_POLLER_TYPE_NFCB:
            
            rfalNfcbPollerInitialize();
            if( gDevList[devIt].dev.nfcb.isSleep )                                    /* Check if desired device is in Sleep */
            {
                /* Wake up all cards. SENSB_RES may return collision but the NFCID0 is available to explicitly select NFC-B card via ATTRIB; so error will be ignored here */
                rfalNfcbPollerCheckPresence( RFAL_NFCB_SENS_CMD_ALLB_REQ, RFAL_NFCB_SLOT_NUM_1, &sensbRes, &sensbResLen );
            }
            
            
            /*******************************************************************************/
            /* Perform ISO-DEP (ISO14443-4) activation: RATS and PPS if supported          */
            err = rfalIsoDepPollBHandleActivation( (rfalIsoDepFSxI)RFAL_ISODEP_FSDI_DEFAULT, RFAL_ISODEP_NO_DID, RFAL_BR_424, 0x00, &gDevList[devIt].dev.nfcb, NULL, 0, &gDevList[devIt].proto.isoDep );
            if( err == ERR_NONE )
            {
                platformLog("NFC-B T4T (ISO-DEP) device activated \r\n");             /* NFC-B T4T device activated */
                
                gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_ISODEP ;
                break;
            }
            
            platformLog("NFC-B device activated \r\n");                               /* NFC-B  device activated */
            gDevList[devIt].rfInterface =  EXAMPLE_RFAL_POLLER_INTERFACE_RF;
            break;
            
        /*******************************************************************************/
        /* NFC-F Activation                                                            */
        /*******************************************************************************/
        case EXAMPLE_RFAL_POLLER_TYPE_NFCF:
            
            rfalNfcfPollerInitialize( RFAL_BR_212 );
            if( rfalNfcfIsNfcDepSupported( &gDevList[devIt].dev.nfcf ) )
            {
                /* Perform NFC-DEP (P2P) activation: ATR and PSL if supported */
                if( !exampleRfalPollerNfcDepActivate( &gDevList[devIt] ) )
                {
                    return false;
                }
                
                platformLog("NFC-F P2P (NFC-DEP) device activated \r\n");             /* NFC-A P2P device activated */
                
                gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_NFCDEP;
                break;
            }
            
            platformLog("NFC-F T3T device activated \r\n");                           /* NFC-F T3T device activated */
            gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_RF;
            break;
            
        /*******************************************************************************/
        /* NFC-V Activation                                                            */
        /*******************************************************************************/
        case EXAMPLE_RFAL_POLLER_TYPE_NFCV:
            
            rfalNfcvPollerInitialize();
            
            /* No specific activation needed for a T5T */
            platformLog("NFC-V T5T device activated \r\n");                           /* NFC-V T5T device activated */
            
            gDevList[devIt].rfInterface = EXAMPLE_RFAL_POLLER_INTERFACE_RF;
            break;
        
        /*******************************************************************************/
        default:
            return false;
    }
    
    gActiveDev = &gDevList[devIt];                                                    /* Assign active device to be used further on */
    return true;
}


/*!
 ******************************************************************************
 * \brief Poller NFC DEP Activate
 * 
 * This method performs NFC-DEP Activation 
 *  
 * \param[in]  devIt : device to be activated 
 * 
 * \return true         : Activation successful 
 * \return false        : Activation failed
 * 
 ******************************************************************************
 */
static bool exampleRfalPollerNfcDepActivate( exampleRfalPollerDevice *device )
{
    rfalNfcDepAtrParam   param;
                
    /*******************************************************************************/
    /* If Passive F use the NFCID2 retrieved from SENSF                            */
    if( device->type == EXAMPLE_RFAL_POLLER_TYPE_NFCF )
    {
        param.nfcid    = device->dev.nfcf.sensfRes.NFCID2;
        param.nfcidLen = RFAL_NFCF_NFCID2_LEN;
    }
    else
    {
        param.nfcid    = gNfcid3;
        param.nfcidLen = RFAL_NFCDEP_NFCID3_LEN;
    }    
    
    param.BS    = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    param.BR    = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    param.DID   = RFAL_NFCDEP_DID_NO;
    param.NAD   = RFAL_NFCDEP_NAD_NO;
    param.LR    = RFAL_NFCDEP_LR_254;
    param.GB    = gGenBytes;
    param.GBLen = sizeof(gGenBytes);
    param.commMode  = RFAL_NFCDEP_COMM_PASSIVE;
    param.operParam = (RFAL_NFCDEP_OPER_FULL_MI_EN | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS | RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);
    
    /* Perform NFC-DEP (P2P) activation: ATR and PSL if supported */
    return (rfalNfcDepInitiatorHandleActivation( &param, RFAL_BR_424, &device->proto.nfcDep ) == ERR_NONE);
}


/*!
 ******************************************************************************
 * \brief Data Exchange
 * 
 * This method performs Data Exchange by device's type and interface.
 *  
 * 
 * \return ERR_REQUEST     : Bad request
 * \return ERR_BUSY        : Data Exchange ongoing
 * \return ERR_NONE        : Data Exchange terminated successfully
 * 
 ******************************************************************************
 */
static ReturnCode exampleRfalPollerDataExchange( void )
{
    rfalTransceiveContext ctx;
    ReturnCode            err;
    rfalIsoDepTxRxParam   isoDepTxRx;
    rfalNfcDepTxRxParam   nfcDepTxRx;
    uint8_t               *txBuf;
    uint16_t              txBufLen;
    uint8_t             response;
    
    
    /*******************************************************************************/
    /* The Data Exchange is divided in two different moments, the trigger/Start of *
     *  the transfer followed by the check until its completion                    */
    if( gState == EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START )                      /* Trigger/Start the data exchange */
    {
        switch( gActiveDev->rfInterface )                                             /* Check which RF interface shall be used/has been activated */
        {
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_RF:
    
                switch( gActiveDev->type )                                            /* Over RF interface no specific protocol is selected, each device supports a different protocol */
                {
                    /*******************************************************************************/
                    case EXAMPLE_RFAL_POLLER_TYPE_NFCA:
                        switch( gActiveDev->dev.nfca.type )
                        {
                            /*******************************************************************************/
                            case RFAL_NFCA_T1T:
                                
                                /* To perform presence check, on this example a T1T Read command is used */
                                ST_MEMCPY( &t1tReadReq[3], gActiveDev->dev.nfca.nfcId1, RFAL_NFCA_CASCADE_1_UID_LEN );  /* Assign device's NFCID for read command */
                                                        
                                txBuf    = t1tReadReq;
                                txBufLen = sizeof(t1tReadReq);
                                break;
                                
                            /*******************************************************************************/
                            case RFAL_NFCA_T2T:
                                
                                /* To perform presence check, on this example a T2T Read command is used */
                                txBuf    = t2tReadReq;
                                txBufLen = sizeof(t2tReadReq);
                                break;
                            
                            /*******************************************************************************/
                            default:
                                return ERR_REQUEST;;
                        }
                        break;

                        
                    /*******************************************************************************/
                    case EXAMPLE_RFAL_POLLER_TYPE_NFCB:
                        
                        /* To perform presence check, no specific command is used */
                        txBuf    = nfcbReq;
                        txBufLen = sizeof(nfcbReq);
                        break;
                        
                        
                    /*******************************************************************************/
                    case EXAMPLE_RFAL_POLLER_TYPE_NFCF:
                        
                        /* To perform presence check, on this example a T3T Check/Read command is used */
                        ST_MEMCPY( &t3tCheckReq[1], gActiveDev->dev.nfcf.sensfRes.NFCID2, RFAL_NFCF_NFCID2_LEN );  /* Assign device's NFCID for Check command */
                        
                        txBuf    = t3tCheckReq;
                        txBufLen = sizeof(t3tCheckReq);
                        break;
                        
                        
                    /*******************************************************************************/
                    case EXAMPLE_RFAL_POLLER_TYPE_NFCV:
                        
                        /* To perform presence check, on this example a Get System Information command is used */

                        txBuf    = t5tSysInfoReq;
                        txBufLen = sizeof(t5tSysInfoReq);
                        break;
                        
                        
                    /*******************************************************************************/
                    default:
                        return ERR_REQUEST;
                }
                
                /*******************************************************************************/
                /* Trigger a RFAL Transceive using the previous defined frames                 */
                rfalCreateByteFlagsTxRxContext( ctx, txBuf, txBufLen, gRxBuf.rfRxBuf, sizeof(gRxBuf.rfRxBuf), &gRcvLen, RFAL_TXRX_FLAGS_DEFAULT, rfalConvMsTo1fc(20) );

                return (((err = rfalStartTransceive( &ctx )) == ERR_NONE) ? ERR_BUSY : err);     /* Signal ERR_BUSY as Data Exchange has been started and is ongoing */
                
            case EXAMPLE_RFAL_POLLER_INTERFACE_ISODEP:
                
                ST_MEMCPY( gTxBuf.isoDepTxBuf.inf, t4tSelectReq, sizeof(t4tSelectReq) );
                
                isoDepTxRx.DID          = RFAL_ISODEP_NO_DID;
                isoDepTxRx.ourFSx       = RFAL_ISODEP_FSX_KEEP;
                isoDepTxRx.FSx          = gActiveDev->proto.isoDep.info.FSx;
                isoDepTxRx.dFWT         = gActiveDev->proto.isoDep.info.dFWT;
                isoDepTxRx.FWT          = gActiveDev->proto.isoDep.info.FWT;
                isoDepTxRx.txBuf        = &gTxBuf.isoDepTxBuf;
                isoDepTxRx.txBufLen     = sizeof(t4tSelectReq);
                isoDepTxRx.isTxChaining = false;
                isoDepTxRx.rxBuf        = &gRxBuf.isoDepRxBuf;
                isoDepTxRx.rxLen        = &gRcvLen;
                isoDepTxRx.isRxChaining = &gRxChaining;
                
                /*******************************************************************************/
                /* Trigger a RFAL ISO-DEP Transceive                                           */
                return (((err = rfalIsoDepStartTransceive( isoDepTxRx )) == ERR_NONE) ? ERR_BUSY : err); /* Signal ERR_BUSY as Data Exchange has been started and is ongoing */
                
                
            case EXAMPLE_RFAL_POLLER_INTERFACE_NFCDEP:
                
                ST_MEMCPY( gTxBuf.nfcDepTxBuf.inf, llcpSymm, sizeof(llcpSymm) );
                
                nfcDepTxRx.DID          = RFAL_NFCDEP_DID_KEEP;
                nfcDepTxRx.FSx          = rfalNfcDepLR2FS( rfalNfcDepPP2LR( gActiveDev->proto.nfcDep.activation.Target.ATR_RES.PPt ) );
                nfcDepTxRx.dFWT         = gActiveDev->proto.nfcDep.info.dFWT;
                nfcDepTxRx.FWT          = gActiveDev->proto.nfcDep.info.FWT;
                nfcDepTxRx.txBuf        = &gTxBuf.nfcDepTxBuf;
                nfcDepTxRx.txBufLen     = sizeof(llcpSymm);
                nfcDepTxRx.isTxChaining = false;
                nfcDepTxRx.rxBuf        = &gRxBuf.nfcDepRxBuf;
                nfcDepTxRx.rxLen        = &gRcvLen;
                nfcDepTxRx.isRxChaining = &gRxChaining;
                
                /*******************************************************************************/
                /* Trigger a RFAL NFC-DEP Transceive                                           */
                return (((err = rfalNfcDepStartTransceive( &nfcDepTxRx )) == ERR_NONE) ? ERR_BUSY : err);  /* Signal ERR_BUSY as Data Exchange has been started and is ongoing */
                
            default:
                break;
        }
    }
    /*******************************************************************************/
    /* The Data Exchange has been started, wait until completed                    */
    else if( gState == EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_CHECK )
    {
        switch( gActiveDev->rfInterface )
        {
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_RF:
                response = rfalGetTransceiveStatus();
                return response;
                
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_ISODEP:
                return rfalIsoDepGetTransceiveStatus();
                
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_NFCDEP:
                return rfalNfcDepGetTransceiveStatus();
                
            /*******************************************************************************/
            default:
                return ERR_PARAM;
        }
    }
    return ERR_REQUEST;
}


/*!
 ******************************************************************************
 * \brief Poller NFC DEP Deactivate
 * 
 * This method Deactivates the device if a deactivation procedure exists 
 * 
 * \return true         : Deactivation successful 
 * \return false        : Deactivation failed
 * 
 ******************************************************************************
 */
static bool exampleRfalPollerDeactivate( void )
{
    if( gActiveDev != NULL )                                                          /* Check if a device has been activated */
    {
        switch( gActiveDev->rfInterface )
        {
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_RF:
                break;                                                                /* No specific deactivation to be performed */
                
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_ISODEP:
                rfalIsoDepDeselect();                                                 /* Send a Deselect to device */
                break;
                
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_INTERFACE_NFCDEP:
                rfalNfcDepRLS();                                                      /* Send a Release to device */
                break;
                
            default:
                return false;
        }
        platformLog("Device deactivated \r\n");
    }
    
    return true;
}


/*!
 *****************************************************************************
 * \brief log to buffer
 *
 * Append log information to actual logging buffer
 * 
 *  \param format   : sprintf compatible string formating
 *  \return size    : size of current log buffer
 * 
 *****************************************************************************
 */
int platformLog(const char* format, ...)
{
	#define TMP_BUFFER_SIZE 256
	char tmpBuffer[TMP_BUFFER_SIZE];

	va_list argptr;
	va_start(argptr, format);
	int cnt = vsnprintf(tmpBuffer, TMP_BUFFER_SIZE, format, argptr);    
	va_end(argptr);  
	  
	int pos = strlen(logBuffer);
	if((pos + cnt) < LOG_BUFFER_SIZE){
	  	strcat(logBuffer, tmpBuffer);
	}

	return pos;
}



void platformLogCreateHeader(char* buf)
{
	strcpy(buf, LOG_HEADER); 
	logCnt++;

	for(uint32_t i = 0; i < logCnt % 22; i++)
		strcat(buf, "."); 

	strcat(buf, "\n\n");	
	platformDelay(40);
}

/*!
 ******************************************************************************
 * \brief xample NFC Detection Routine
 * 
 * This method implements a state machine going thought all the 
 * different modes that a Reader needs to perform to identify differnet cards
 * 
 * 
 ******************************************************************************
 */
 
extern void exampleNFCDetection( void )
{
    ReturnCode err;
    uint8_t    i;
    
    rfalAnalogConfigInitialize();                                                     /* Initialize RFAL's Analog Configs */
    rfalInitialize();                                                                 /* Initialize RFAL */
   
	for(;;)
	{

	    rfalWorker();                                                                 /* Execute RFAL process */

	    /* switchoff all the leds at start */
      platformDelay(20);

	    switch( gState )
	    {
	        /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_INIT:                                     
	            
	            gTechsFound = EXAMPLE_RFAL_POLLER_FOUND_NONE; 
	            gActiveDev  = NULL;
	            gDevCnt     = 0;
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_TECHDETECT;
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_TECHDETECT:
	            
	            if( !exampleRfalPollerTechDetetection() )                             /* Poll for nearby devices in different technologies */
	            {
	                gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If no device was found, restart loop */
	                break;
	            }
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE;                      /* One or more devices found, go to Collision Avoidance */
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE:
	            
	            if( !exampleRfalPollerCollResolution() )                              /* Resolve any eventual collision */
                {
                    gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If Collision Resolution was unable to retrieve any device, restart loop */
                    break;
                }
	            
	            platformLog("Device(s) found: %d \r\n", gDevCnt);
	            
	            for(i=0; i<gDevCnt; i++)
	            {
	                switch( gDevList[i].type )
	                {
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCA:
	                        platformLog( " NFC-A device UID: %s \r\n", hex2str(gDevList[i].dev.nfca.nfcId1, gDevList[i].dev.nfca.nfcId1Len) );
	                        break;
	                        
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCB:
	                        platformLog( " NFC-B device UID: %s \r\n", hex2str(gDevList[i].dev.nfcb.sensbRes.nfcid0, RFAL_NFCB_NFCID0_LEN) );
                            break;
                            
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCF:
	                        platformLog( " NFC-F device UID: %s \r\n", hex2str(gDevList[i].dev.nfcf.sensfRes.NFCID2, RFAL_NFCF_NFCID2_LEN) );
                            break;
                            
	                    case EXAMPLE_RFAL_POLLER_TYPE_NFCV:
	                        platformLog( " NFC-V device UID: %s \r\n", hex2str(gDevList[i].dev.nfcv.InvRes.UID, RFAL_NFCV_UID_LEN) );
                            break;
	                }

	            }
	            gState = EXAMPLE_RFAL_POLLER_STATE_ACTIVATION;                        /* Device(s) have been identified, go to Activation */
                break;
	        
                
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_ACTIVATION:
	        case EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION:
          
	            rfalFieldOff();                                                       /* Turn the Field Off powering down any device nearby */
	            platformDelay(2);                                                     /* Remain a certain period with field off */
	            gState = EXAMPLE_RFAL_POLLER_STATE_INIT;                              /* Restart the loop */

                platformLogClear();
                platformLog2Screen(logBuffer);

	            break;
	        
	        
            /*******************************************************************************/
	        default:
	            return;
	    }
	}
}

/*!
 ******************************************************************************
 * \brief Select the type of NFC to be scanned for
 * 
 * This method takes user inp[ut and selects the specific card type to be scanned
 * 
 * \return type         : the chosen NFC type or none if not required
 * 
 ******************************************************************************
 */
static int selectNFCType(void)
{
    char option;
    exampleRfalPollerDevType choice = EXAMPLE_RFAL_POLLER_TYPE_NONE;
    
    /* Present the options to the user */
    printf(" \n\n");
    printf("**************************************************************************\n");
    printf("Available NFC Types: -\n\n");
    printf("A - NFC-A device\n");
    printf("B - NFC-B device\n");
    printf("F - NFC-F device\n");
    printf("V - NFC-V device\n");
    printf("e - return \n");
    printf(" \n");

    printf("Please select choice -> ");

    option = getchar();
    getchar();  // have to press enter and this consumes the enter character
    switch (tolower(option))
    {
        case 'a':
            choice = EXAMPLE_RFAL_POLLER_TYPE_NFCA;
            break;
        case 'b':
            choice = EXAMPLE_RFAL_POLLER_TYPE_NFCB;
            break;
        case 'f':
            choice = EXAMPLE_RFAL_POLLER_TYPE_NFCF;
            break;
        case 'v':
            choice = EXAMPLE_RFAL_POLLER_TYPE_NFCV;
            break;
        default:
            choice = EXAMPLE_RFAL_POLLER_TYPE_NONE;
            break;
        }
    return choice;
}

/*!
 ******************************************************************************
 * \brief Scan for the given type of NFC to be scanned for
 * 
 * This method takes the given type and returns once it has been scanned
 * 
 ******************************************************************************
 */
static void scanforNFCType(exampleRfalPollerDevType device_type)
{
    uint8_t    i;

    bool tag_found = false;                                 /* Used to identify when the right tag type has been detected */
    bool finished = false;
    
    rfalAnalogConfigInitialize();                                                     /* Initialize RFAL's Analog Configs */
    rfalInitialize();                                                                 /* Initialize RFAL */

	do
	{
	    rfalWorker();                                                                 /* Execute RFAL process */


      platformDelay(20);

	    switch( gState )
	    {
	        /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_INIT:                                     
	            
	            gTechsFound = EXAMPLE_RFAL_POLLER_FOUND_NONE; 
	            gActiveDev  = NULL;
	            gDevCnt     = 0;
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_TECHDETECT;
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_TECHDETECT:
	            
	            if( !exampleRfalPollerTechDetetection() )                             /* Poll for nearby devices in different technologies */
	            {
	                gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If no device was found, restart loop */
	                break;
	            }
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE;                      /* One or more devices found, go to Collision Avoidance */
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE:
	            
	            if( !exampleRfalPollerCollResolution() )                              /* Resolve any eventual collision */
                {
                    gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If Collision Resolution was unable to retrieve any device, restart loop */
                    break;
                }
	            
	            platformLog("Device(s) found: %d \r\n", gDevCnt);

	            for(i=0; i<gDevCnt; i++)
	            {
                    if (gDevList[i].type == device_type)
                    {
                        tag_found = true;
                        switch( device_type )
                        {
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCA:
                                platformLog( " NFC-A device UID: %s \r\n", hex2str(gDevList[i].dev.nfca.nfcId1, gDevList[i].dev.nfca.nfcId1Len) );
                                break;
                                
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCB:
                                platformLog( " NFC-B device UID: %s \r\n", hex2str(gDevList[i].dev.nfcb.sensbRes.nfcid0, RFAL_NFCB_NFCID0_LEN) );
                                break;
                                
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCF:
                                platformLog( " NFC-F device UID: %s \r\n", hex2str(gDevList[i].dev.nfcf.sensfRes.NFCID2, RFAL_NFCF_NFCID2_LEN) );
                                break;
                                
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCV:
                                platformLog( " NFC-V device UID: %s \r\n", hex2str(gDevList[i].dev.nfcv.InvRes.UID, RFAL_NFCV_UID_LEN) );
                                break;
                        }
                        platformDelay(20);



                        
                    }

	            }
	            gState = EXAMPLE_RFAL_POLLER_STATE_ACTIVATION;                        /* Device(s) have been identified, go to Activation */
                break;
	        
                
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_ACTIVATION:
	        case EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION:
          
	            rfalFieldOff();                                                       /* Turn the Field Off powering down any device nearby */

	            platformDelay(2);                                                     /* Remain a certain period with field off */
	            gState = EXAMPLE_RFAL_POLLER_STATE_INIT;                              /* Restart the loop */

                if (tag_found == true)
                    finished = true;
                    
                platformLogClear();
                platformLog2Screen(logBuffer);

	            break;
	        
	        
            /*******************************************************************************/
	        default:
	            return;
	    }
	} while (finished == false);

}

/*!
 ******************************************************************************
 * \brief Read the memory for the given type of NFC
 * 
 * This method takes the given type and returns once it has read the memory
 * 
 ******************************************************************************
 */

static void readNFCMemory(exampleRfalPollerDevType device_type)
{
    uint8_t    i;
    ReturnCode            err;
    bool memory_read = false;                                 /* Used to identify when the right tag type has been detected */
    bool finished = false;
    uint8_t     position = 0;                               /* The position in the devices detected to be read */
    
    rfalAnalogConfigInitialize();                                                     /* Initialize RFAL's Analog Configs */
    rfalInitialize();                                                                 /* Initialize RFAL */
   
    do
	{

	    rfalWorker();                                                                 /* Execute RFAL process */

      platformDelay(20);

	    switch( gState )
	    {
	        /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_INIT:                                     
	            
	            gTechsFound = EXAMPLE_RFAL_POLLER_FOUND_NONE; 
	            gActiveDev  = NULL;
	            gDevCnt     = 0;
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_TECHDETECT;
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_TECHDETECT:
	            
	            if( !exampleRfalPollerTechDetetection() )                             /* Poll for nearby devices in different technologies */
	            {
	                gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If no device was found, restart loop */
	                break;
	            }
	            
	            gState = EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE;                      /* One or more devices found, go to Collision Avoidance */
	            break;
	            
	            
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_COLAVOIDANCE:
	            
	            if( !exampleRfalPollerCollResolution() )                              /* Resolve any eventual collision */
                {
                    gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If Collision Resolution was unable to retrieve any device, restart loop */
                    break;
                }
	            
	            platformLog("Device(s) found: %d \r\n", gDevCnt);

	            for(i=0; i<gDevCnt; i++)
	            {
                    if (gDevList[i].type == device_type)
                    {
                        switch( device_type )
                        {
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCA:
                                platformLog( " NFC-A device UID: %s \r\n", hex2str(gDevList[i].dev.nfca.nfcId1, gDevList[i].dev.nfca.nfcId1Len) );
                                break;
                                
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCB:
                                platformLog( " NFC-B device UID: %s \r\n", hex2str(gDevList[i].dev.nfcb.sensbRes.nfcid0, RFAL_NFCB_NFCID0_LEN) );
                                break;
                                
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCF:
                                platformLog( " NFC-F device UID: %s \r\n", hex2str(gDevList[i].dev.nfcf.sensfRes.NFCID2, RFAL_NFCF_NFCID2_LEN) );
                                break;
                                
                            case EXAMPLE_RFAL_POLLER_TYPE_NFCV:
                                platformLog( " NFC-V device UID: %s \r\n", hex2str(gDevList[i].dev.nfcv.InvRes.UID, RFAL_NFCV_UID_LEN) );
                                break;
                        }
                        position = i;
                    }

	            }
	            gState = EXAMPLE_RFAL_POLLER_STATE_ACTIVATION;                        /* Device(s) have been identified, go to Activation */
                break;

            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_ACTIVATION:

	            if( !exampleRfalPollerActivation( position ) )                               /* Any device previous identified can be Activated.  */
	            {
                    gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                  /* If Activation failed, restart loop */
                    position = 0;
                    break;
                }
	            gState = EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START;                /* Device has been properly activated, go to Data Exchange */
		    break;
            
	            
            /*******************************************************************************/
            case EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START:                       
	        case EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_CHECK:

	            err = exampleRfalPollerDataExchange();                                /* Perform Data Exchange, in this example a simple transfer will executed in order to do device's presence check */
                switch( err )
                {
                    case ERR_NONE:                                                    /* Data exchange successful  */
                        platformDelay(300);                                           /* Wait a bit */
                        //gState = EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_START;        /* Trigger new exchange with device */
                        gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;                /* Trigger new exchange with device - changed by MB*/
                        break;
                        
                    case ERR_BUSY:                                                    /* Data exchange ongoing  */
                        gState = EXAMPLE_RFAL_POLLER_STATE_DATAEXCHANGE_CHECK;        /* Once triggered/started the Data Exchange only do check until is completed */
                        memory_read = true;
                        break;
                        
                    default:                                                          /* Data exchange not successful, card removed or other transmission error */
                        gState = EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION;              /* Restart loop */
                        break;
                }
                break;
                
            /*******************************************************************************/
	        case EXAMPLE_RFAL_POLLER_STATE_DEACTIVATION:

	            rfalFieldOff();                                                       /* Turn the Field Off powering down any device nearby */
	            gState = EXAMPLE_RFAL_POLLER_STATE_INIT;                              /* Restart the loop */

                if (memory_read == true)
                    finished = true;
                            
                platformLogClear();
                platformLog2Screen(logBuffer);

	            break;
	        
	        
            /*******************************************************************************/

	        default:
	            return;
	    }
	} while (finished != true);


}

/*!
 ******************************************************************************
 * \brief Communicate with a NFC-V type tag and read block zero.
 * 
 * This method displays the data in block zero
 * 
 * \return              : nothing
 * 
 ******************************************************************************
 */

static void readNFCVSingleBlock(void)
{
    /* Setup the required variables     */
    ReturnCode ret;                          /* The value returned from the various functions */
    uint8_t i, j;                              /* Counter */
    uint16_t    rcvdLen;                       /* The number of bits received without collision */
    uint8_t devLimit = 10;                  /* The maximum number of devices to detect */
    rfalNfcvListenDevice nfcvDevList[devLimit];      /* Device Structure. */
    uint8_t devCnt = 0;                          /* Count of the devices found */
    uint16_t rxBufLen = 32;                          /* Length of the rxbuf */
    uint8_t rxBuf[rxBufLen];                   /* Where the received information is stored */
    uint16_t rcvLen;                           /* Received length of data */
    rfalNfcvInventoryRes invRes;               /* inventory list */
    bool found = false;                           /* Indicator if a item has been found and hence to continue */
      

    /* Initialisation */
    printf("Initialising the chip for NFC V tags\n");

    rfalAnalogConfigInitialize();                                          /* Initialize RFAL's Analog Configs */
    rfalInitialize();                                                      /* Initialize RFAL */

    ret = rfalNfcvPollerInitialize();
    if (ret != ERR_NONE)
    {
        printf("Failed to Initialize:%s\n", ret);
        return;
    }
    rfalFieldOnAndStartGT();                                               /* Turns the Field on if not already and start GT Timer */

    /* Check Presence */                                                /* What if multiple tags??*/
    printf("Checking for a tag in the field (CTRL-C to exit)\n");
    do
    {
        ret = rfalNfcvPollerCheckPresence( &invRes );
        if (ret == ERR_NONE)
        {
            found = true;
        }
    } while (found != true);

    /* Collision Resolution */
    printf("Performing Collision Resolution\n");
    ret = rfalNfcvPollerCollisionResolution( devLimit, nfcvDevList, &devCnt );
    if (ret != ERR_NONE)
    {
        printf("Failed to complete collision resolution:%d\n", ret);
        return;
    }
    else
    {
        printf("Device Count:%d\n", devCnt);
        if (devCnt > 0)
        {
            for (i=0; i < devCnt; i++)
            {
                printf("UID:");
                for (j=0; j < RFAL_NFCV_UID_LEN; j++)
                {
                    printf("%x", *(nfcvDevList[i].InvRes.UID + j));
                }
                printf("\n");
            }
        }
        else
        {
            /* No devices found during collision resolution */
            printf("Communication with tag lost during Collision Resolution, memory read aborted\n");
            return;
        }          
    }
    
    /* Select tag */
    printf("Reading the first tag found\n");
    ret = rfalNfvPollerSelect( RFAL_NFCV_REQ_FLAG_DEFAULT, nfcvDevList[0].InvRes.UID );
    if (ret != ERR_NONE)
    {
        printf("Failed to complete tag Selection Selection with the following error code:%d\n", ret);
        return;
    }
    
    /* Read Single Block */
    ret = rfalNfvPollerReadSingleBlock( RFAL_NFCV_REQ_FLAG_DEFAULT, NULL, 0, rxBuf, sizeof(rxBuf), &rcvLen );
    if (ret != ERR_NONE)
    {
        printf("Failed to Read block of data for the tag:%d\n", ret);
        return;
    }
    else
    {
        if (rcvLen > 0)
        {
            printf("Data Received:");
            for (j=1; j < rcvLen; j++)                      /* rxBuf contains flags plus 4 bytes of data */
            {
                printf("%02x", *(rxBuf + j));
            }
            printf("\n");
        }
    }
}


/*!
 ******************************************************************************
 * \brief Communicate with a NFC-V type tag and write to block zero.
 * 
 * This method first reads and then writes a value to block zero
 * 
 * \return              : nothing
 * 
 ******************************************************************************
 */
static void writeNFCVSingleBlock(void)
{
    /* Setup the required variables     */
    ReturnCode ret;                                  /* The value returned from the various functions */
    uint8_t i, j;                                   /* Counter */
    uint16_t    rcvdLen;                            /* The number of bits received without collision */
    uint8_t devLimit = 10;                          /* The maximum number of devices to detect */
    rfalNfcvListenDevice nfcvDevList[devLimit];    /* Device Structure. */
    uint8_t devCnt = 0;                             /* Count of the devices found */
    uint16_t rxBufLen = 32;                         /* Length of the rxbuf */
    uint8_t rxBuf[rxBufLen];                        /* Where the received information is stored */
    uint16_t rcvLen;                                /* Received length of data */
    rfalNfcvInventoryRes invRes;                   /* inventory list */
    bool found = false;                              /* Indicator if a item has been found and hence to continue */
    uint8_t blockLen = 4;                           /* The length of the block to be written */
    uint8_t wrData[blockLen];                       /* Data to be written to the block */

    /* Initialisation */
    printf("Initialising the chip for NFC V tags\n");

    rfalAnalogConfigInitialize();                                          /* Initialize RFAL's Analog Configs */
    rfalInitialize();                                                      /* Initialize RFAL */

    // The following line was added from community forum, not sure it is needed though.
    // It may need to be after PollerInitialize() though

    ret = rfalNfcvPollerInitialize();
    if (ret != ERR_NONE)
    {
        printf("Failed to Initialize:%s\n", ret);
        return;
    }
    rfalFieldOnAndStartGT();                                               /* Turns the Field on if not already and start GT Timer */

    /* Check Presence */                                                /* What if multiple tags??*/
    printf("Checking for a tag in the field (CTRL-C to exit)\n");
    do
    {
        ret = rfalNfcvPollerCheckPresence( &invRes );
        if (ret == ERR_NONE)
        {
            found = true;
        }

    } while (found != true);

    /* Collision Resolution */
    printf("Performing Collision Resolution\n");
    ret = rfalNfcvPollerCollisionResolution( devLimit, nfcvDevList, &devCnt );
    if (ret != ERR_NONE)
    {
        printf("Failed to complete collision resolution:%d\n", ret);
        return;
    }
    else
    {
        printf("Device Count:%d\n", devCnt);
        if (devCnt > 0)
        {
            for (i=0; i < devCnt; i++)
            {
                //printf("Here-1\n");
                printf("UID:");
                for (j=0; j < RFAL_NFCV_UID_LEN; j++)
                {
                    //printf("Here-2\n");
                    printf("%x", *(nfcvDevList[i].InvRes.UID + j));
                }
                printf("\n");
            }
        }
        else
        {
            /* No devices found during collision resolution */
            printf("Communication with tag lost during Collision Resolution, memory read aborted\n");
            return;
        }          
    }
    
    /* Select tag */
    printf("Reading the first tag found\n");
    ret = rfalNfvPollerSelect( RFAL_NFCV_REQ_FLAG_DEFAULT, nfcvDevList[0].InvRes.UID );
    if (ret != ERR_NONE)
    {
        printf("Failed to complete tag Selection Selection with the following error code:%d\n", ret);
        return;
    }

    /* Read Single Block beforehand*/
    ret = rfalNfvPollerReadSingleBlock( RFAL_NFCV_REQ_FLAG_DEFAULT, NULL, 0, rxBuf, sizeof(rxBuf), &rcvLen );
    if (ret != ERR_NONE)
    {
        printf("Failed to Read block of data for the tag:%d\n", ret);
        return;
    }
    else
    {
        if (rcvLen > 0)
        {
            /* Write the data to the console */
            printf("Data Before Write:");
            for (j=1; j < rcvLen; j++)                      /* rxBuf contains flags plus 4 bytes of data, hence only printing from posn 1 */
            {
                printf("%02x", *(rxBuf + j));
            }
            printf("\n");
        }
    }
    /* Write to block zero */
    /* Create the data to be written */
    /* Take the data read and add 1 to each of the values to be written
     * rxBuf contains flags plus 4 bytes of data, wrData only wants the data to be written */
    
    for (j=0; j < blockLen; j++)
    {
        wrData[j] = 1 + *(rxBuf + (j+1));                    /* adding 1 to value written from rxBuf due to byte zero being the flags  */
        if (wrData[j] > 255)
        {
            /* if the value is greater than 0xFF, reset it to zero */
            wrData[j] = 0;
        }
    }

    /* Write the data to the block zero */
    ret = rfalNfvPollerWriteSingleBlock( RFAL_NFCV_REQ_FLAG_DEFAULT, NULL, 0, wrData, 4);
    if (ret != ERR_NONE)
    {
        printf("Failed to Write to block 0 for the tag:%d\n", ret);
        return;
    }
    else
    {
        printf("Data Written successfully\n");
    }    

    /* Read Single Block afterwards*/
    ret = rfalNfvPollerReadSingleBlock( RFAL_NFCV_REQ_FLAG_DEFAULT, NULL, 0, rxBuf, sizeof(rxBuf), &rcvLen );
    if (ret != ERR_NONE)
    {
        printf("Failed to Read block of data for the tag:%d\n", ret);
        return;
    }
    else
    {
        if (rcvLen > 0)
        {
            printf("Data After Write:");
            for (j=1; j < rcvLen; j++)                      /* rxBuf contains flags plus 4 bytes of data */
            {
                printf("%02x", *(rxBuf + j));
            }
            printf("\n");
        }
    }
}


/*!
 ******************************************************************************
 * \brief Communicate with a NFC-V type tag and read block zero.
 * 
 * This method displays the data in block zero
 * 
 * \return              : nothing
 * 
 ******************************************************************************
 */

/*!
 ******************************************************************************
 * \brief Hardware Setup of GPIO and SPI
 * 
 * This method sets up the GPIO and SPI 
 * 
 * \return true         : Setup successful 
 * \return false        : Setup failed
 * 
 ******************************************************************************
 */

 static bool HardwareInitialisation( void)
 {
     int resp;
    /* Initialize the platform */
	/* Initialize GPIO */
  	resp = gpio_init();
	if(resp != ERR_NONE)
		return false;
        
	/* Initialize SPI */
	resp = spi_init();
	if(resp != ERR_NONE)
		return false;
        
	/* Initialize interrupt mechanism */
	resp = interrupt_init();
	if (resp != ERR_NONE) {
		return false;
  }

    return true;
    }
    
/*
 ******************************************************************************
 * MAIN FUNCTION
 ******************************************************************************
 */
int main(void)
{
	setlinebuf(stdout);
	int ret = 0;
    char option;
    exampleRfalPollerDevType type = EXAMPLE_RFAL_POLLER_FOUND_NONE;
    
    splashscreen();

    ret = HardwareInitialisation();
    if (ret != true)
        return ret;

    do {
        printf(" \n\n");
        printf("**************************************************************************\n");
        printf("Available commands: -\n\n");
        printf("a - Scan for available cards\n");
        printf("s - Scan for specific card type\n");
        printf("m - Example Read card memory (ST Example)\n");
        printf("v - Read Block Zero from first NFC-V tag found\n");
        printf("w - Write to Block Zero on the first NFC-V tag found\n");
        printf("e - Exit program \n");
        printf(" \n");

        printf("Please select command -> ");

        option = getchar();
        getchar();  // have to press enter and this consumes the enter character


        switch (option)
        {

            case 'a': // Scan for available cards

                /* Initialize rfal and run example code for NFC */
                exampleNFCDetection();
                break;
            case 's': // Scan for specific cards
                /* Select the NFC Type Rquired */
                type = selectNFCType();
                
                /* Scan for this card type */
                scanforNFCType(type);
                break;
            case 'm': // Read the card memory
                /* Select the NFC Type Rquired */
                type = selectNFCType();
                
                /* Scan for this card type */
                readNFCMemory(type);
                break;
            case 'v': // Communicate with NFC V tag (ICode)
                readNFCVSingleBlock();
                break;
            case 'w': // Communicate with NFC V tag (ICode)
                writeNFCVSingleBlock();
                break;
            case 'e':
                printf("Exiting.......\n");
                option = 'e';
                break;

            default:
                printf("Unrecognised command!\n");

       }
       fflush (stdout) ;

    } while(option != 'e');
}
