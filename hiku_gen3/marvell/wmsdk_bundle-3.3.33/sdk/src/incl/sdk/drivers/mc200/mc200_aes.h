/****************************************************************************//**
 * @file     mc200_aes.h
 * @brief    AES driver module header file.
 * @version  V1.2.0
 * @date     29-May-2013
 * @author   CE Application Team
 *
 * @note
 * Copyright (C) 2012 Marvell Technology Group Ltd. All rights reserved.
 *
 * @par
 * Marvell is supplying this software which provides customers with programming
 * information regarding the products. Marvell has no responsibility or 
 * liability for the use of the software. Marvell not guarantee the correctness 
 * of this software. Marvell reserves the right to make changes in the software 
 * without notification. 
 * 
 *******************************************************************************/

#ifndef __MC200_AES_H__
#define __MC200_AES_H__

#include "mc200.h"
#include "mc200_driver.h"

/** @addtogroup  MC200_Periph_Driver
 *  @{
 */

/** @addtogroup  AES 
 *  @{
 */
  
/** @defgroup AES_Public_Types AES_Public_Types
 *  @brief AES configuration structure type definition
 *  @{
 */
 
/**  
 *  @brief AES mode type definition 
 */
typedef enum
{
  AES_MODE_ECB    = 0,	                /*!< AES mode: ECB */
  AES_MODE_CBC    = 1,                  /*!< AES mode: CBC */
  AES_MODE_CTR    = 2,                  /*!< AES mode: CTR */
  AES_MODE_CCM    = 5,                  /*!< AES mode: CCM */ 
  AES_MODE_MMO    = 6,                  /*!< AES mode: MMO */
  AES_MODE_BYPASS = 7,                  /*!< AES mode: Bypass */
}AES_Mode_Type;

/**  
 *  @brief AES enc/dec select type definition 
 */
typedef enum
{
  AES_MODE_ENCRYPTION    = 0,           /*!< AES ENC/DEC select: ENCRYPTION */
  AES_MODE_DECRYPTION    = 1,           /*!< AES ENC/DEC select: DECRYPTION */
}AES_EncDecSel_Type;

/**  
 *  @brief AES key size type definition 
 */
typedef enum
{
  AES_KEY_BYTES_16       = 0,           /*!< AES key size select: 16 bytes */
  AES_KEY_BYTES_32       = 1,           /*!< AES key size select: 32 bytes */
  AES_KEY_BYTES_24       = 2,           /*!< AES key size select: 24 bytes */
}AES_KeySize_Type;

/**  
 *  @brief AES MIC size type definition 
 */
typedef enum
{
  AES_MIC_BYTES_0        = 0,           /*!< AES MIC length: 0 bytes */
  AES_MIC_BYTES_4        = 1,           /*!< AES MIC length: 4 bytes */
  AES_MIC_BYTES_8        = 2,           /*!< AES MIC length: 8 bytes */
  AES_MIC_BYTES_16       = 3,           /*!< AES MIC length: 16 bytes */    
}AES_MICLen_Type;

/**  
 *  @brief AES interrupt type definition 
 */
typedef enum
{
  AES_INT_OUTFIFO_EMPTY,                /*!< AES output FIFO empty int flag */
  AES_INT_INFIFO_FULL,                  /*!< AES input FIFO full int flag */
  AES_INT_DONE,                         /*!< AES done int flag */
  AES_INT_ALL,                          /*!< All AES interrupt flag */
}AES_INT_Type; 

/**  
 *  @brief AES status type definition 
 */
typedef enum
{
  AES_STATUS_OUTFIFO_EMPTY,             /*!< AES output FIFO empty flag */
  AES_STATUS_INFIFO_FULL,               /*!< AES input FIFO full flag */
  AES_STATUS_OUTFIFO_RDY,               /*!< AES output FIFO can be read flag */
  AES_STATUS_DONE,                      /*!< AES done flag */
  AES_STATUS_ERROR_0,                   /*!< Input stream size is less than 16 bytes in CBC&CTR mode */
  AES_STATUS_ERROR_1,                   /*!< Data is more than 2^13-1 bytes in MMO mode 
					     Data is not multiple of 16 bytes in ECB mode*/
  AES_STATUS_ERROR_2,                   /*!< MIC mismatch during decryption in CCM* mode */
}AES_Status_Type; 

/**  
 *  @brief AES config type definition 
 */
typedef struct
{
  AES_Mode_Type mode;                   /*!< AES mode */
  AES_EncDecSel_Type encDecSel;         /*!< AES ENC/DEC select */
  uint32_t initVect[4];                 /*!< AES initial vector variable */
  AES_KeySize_Type keySize;             /*!< key size variable */
  uint32_t key[8];                      /*!< AES key */
  uint32_t aStrLen;                     /*!< AES A_string length */
  uint32_t mStrLen;                     /*!< AES M_string length */
  AES_MICLen_Type micLen;               /*!< AES MIC length */
  FunctionalState micEn;                /*!< AES MIC Enable/Disable control */ 
}AES_Config_Type;

/*@} end of group AES_Public_Types definitions */


/** @defgroup AES_Public_Constants
 *  @{
 */ 


/*@} end of group AES_KEY_SIZE */


/*@} end of group AES_Public_Constants */

/** @defgroup AES_Public_Macro
 *  @{
 */


/*@} end of group AES_Public_Macro */

/** @defgroup AES_Public_Function_Declaration
 *  @brief AES functions statement
 *  @{
 */

void AES_Init(AES_Config_Type * aesConfig);
void AES_SetIV(const uint32_t *pIV);
void AES_SetMsgLen(uint32_t msgLen);
void AES_ReadOutputVector(uint32_t *pIV);
void AES_CopyOV_to_IV(void);
void AES_Reset(void);
void AES_Enable(void);
void AES_Disable(void);

/* wmsdk: added for CTR mode of AES */
uint32_t AES_GetCTRCntMod(void);
void AES_SetCTRCntMod(uint32_t cntmod);
void AES_OutmsgCmd(FunctionalState state);
void AES_DmaCmd(FunctionalState state);
void AES_FeedData(uint32_t data);
uint32_t AES_ReadData(void);
void AES_ReadMIC(uint32_t* micData, uint8_t len);
void AES_ClrInputFIFO(void);
void AES_ClrOutputFIFO(void);

void AES_IntMask(AES_INT_Type intType, IntMask_Type maskState);
IntStatus AES_GetIntStatus(AES_INT_Type intType);
FlagStatus AES_GetStatus(AES_Status_Type statusType);

void AES_IntClr(AES_INT_Type intType);

void AES_IRQHandler(void);

/*@} end of group AES_Public_Function_Declaration */

/*@} end of group AES  */

/*@} end of group MC200_Periph_Driver */
#endif /* __MC200_AES_H__ */

