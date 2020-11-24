/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio
              Copyright (C) 2020 Matthew Trescott <matthewtrescott@gmail.com>

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    hal_can_lld.h
 * @brief   PLATFORM CAN subsystem low level driver header.
 *
 * @addtogroup CAN
 * @{
 */

#ifndef HAL_CAN_LLD_H
#define HAL_CAN_LLD_H

#if (HAL_USE_CAN == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Number of transmit mailboxes.
 */
#define CAN_TX_MAILBOXES            1

/**
 * @brief   Number of receive mailboxes.
 */
#define CAN_RX_MAILBOXES            1

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    PLATFORM configuration options
 * @{
 */
/**
 * @brief   CAN1 driver enable switch.
 * @details If set to @p TRUE the support for CAN1 is included.
 * @note    The default is @p FALSE.
 */
#if !defined(TIVA_CAN_USE_CAN1) || defined(__DOXYGEN__)
#define TIVA_CAN_USE_CAN1                  FALSE
#endif

/**
 * @brief   CAN2 driver enable switch.
 * @details If set to @p TRUE the support for CAN2 is included.
 * @note    The default is @p FALSE.
 */
#if !defined(TIVA_CAN_USE_CAN1) || defined(__DOXYGEN__)
#define TIVA_CAN_USE_CAN2                  FALSE
#endif
/** @} */

/**
 * @brief   CAN1 interrupt priority level setting.
 * 
 * Acceptable values are in the range [0..7] where 0 is highest priority.
 * @note    The default value is 7, but this is arbitrary.
 */
#if !defined(TIVA_CAN_CAN1_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define TIVA_CAN_CAN1_IRQ_PRIORITY         7
#endif
/** @} */

/**
 * @brief   CAN2 interrupt priority level setting.
 * 
 * Acceptable values are in the range [0..7] where 0 is highest priority.
 * @note    The default value is 7, but this is arbitrary.
 */
#if !defined(TIVA_CAN_CAN2_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define TIVA_CAN_CAN2_IRQ_PRIORITY         7
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !TIVA_CAN_USE_CAN1 && !TIVA_CAN_USE_CAN2
#error "CAN driver activated but no CAN peripheral assigned"
#endif

#if !defined(TIVA_HAS_CAN0)
#error "TIVA_HAS_CAN0 not defined in registry"
#endif

#if !defined(TIVA_HAS_CAN1)
#error "TIVA_HAS_CAN1 not defined in registry"
#endif

#if TIVA_CAN_USE_CAN1 && !TIVA_HAS_CAN0
#error "CAN1 not present in the selected device"
#endif

#if TIVA_CAN_USE_CAN2 && !TIVA_HAS_CAN1
#error "CAN2 not present in the selected device"
#endif

#if TIVA_CAN_USE_CAN1 && \
    !OSAL_IRQ_IS_VALID_PRIORITY(TIVA_CAN_CAN1_IRQ_PRIORITY)
#error "Invalid IRQ priority assigned to CAN1"
#endif
    
#if TIVA_CAN_USE_CAN2 && \
    !OSAL_IRQ_IS_VALID_PRIORITY(TIVA_CAN_CAN2_IRQ_PRIORITY)
#error "Invalid IRQ priority assigned to CAN2"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of a structure representing an CAN driver.
 */
typedef struct CANDriver CANDriver;

/**
 * @brief   Type of a transmission mailbox index.
 */
typedef uint32_t canmbx_t;

#if (CAN_ENFORCE_USE_CALLBACKS == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Type of a CAN notification callback.
 *
 * @param[in] canp      pointer to the @p CANDriver object triggering the
 *                      callback
 * @param[in] flags     flags associated to the mailbox callback
 */
typedef void (*can_callback_t)(CANDriver *canp, uint32_t flags);
#endif

/**
 * @brief   CAN transmission frame.
 * @note    Accessing the frame data as word16 or word32 is not portable because
 *          machine data endianness, it can be still useful for a quick filling.
 */
typedef struct {
  /*lint -save -e46 [6.1] Standard types are fine too.*/
  uint8_t                   DLC:4;          /**< @brief Data length.        */
  uint8_t                   RTR:1;          /**< @brief Frame type.         */
  uint8_t                   IDE:1;          /**< @brief Identifier type.    */
  union {
    uint32_t                SID:11;         /**< @brief Standard identifier.*/
    uint32_t                EID:29;         /**< @brief Extended identifier.*/
    uint32_t                _align1;
  };
  /*lint -restore*/
  union {
    uint8_t                 data8[8];       /**< @brief Frame data.         */
    uint16_t                data16[4];      /**< @brief Frame data.         */
    uint32_t                data32[2];      /**< @brief Frame data.         */
  };
} CANTxFrame;

/**
 * @brief   CAN received frame.
 * @note    Accessing the frame data as word16 or word32 is not portable because
 *          machine data endianness, it can be still useful for a quick filling.
 */
typedef struct {
  /*lint -save -e46 [6.1] Standard types are fine too.*/
  uint8_t                   FMI;            /**< @brief Filter id.          */
  uint16_t                  TIME;           /**< @brief Time stamp.         */
  uint8_t                   DLC:4;          /**< @brief Data length.        */
  uint8_t                   RTR:1;          /**< @brief Frame type.         */
  uint8_t                   IDE:1;          /**< @brief Identifier type.    */
  union {
    uint32_t                SID:11;         /**< @brief Standard identifier.*/
    uint32_t                EID:29;         /**< @brief Extended identifier.*/
    uint32_t                _align1;
  };
  /*lint -restore*/
  union {
    uint8_t                 data8[8];       /**< @brief Frame data.         */
    uint16_t                data16[4];      /**< @brief Frame data.         */
    uint32_t                data32[2];      /**< @brief Frame data.         */
  };
} CANRxFrame;

/* TODO: Add a filter struct also */

/**
 * @brief   Driver configuration structure.
 */
typedef struct {
  /**
   * @brief Bus bitrate in bits/second
   * 
   * @note This is only used if bittime_autoguess is true.
   */
  uint32_t                  bitrate;
  /**
   * @brief Maximum oscillator tolerance between this node and another
   * 
   * Expressed in parts per million (ppm), for accuracy. There are
   * often many combinations of parameters that produce a bitrate suitably
   * close to the nominal bitrate. This parameter gives the auto-guessing
   * algorithm a way to check whether the synchronization jump width is enough
   * to prevent bit errors.
   * 
   * For example, if this chip's oscillator and another chip's oscillator both
   * have 1.25% tolerance, you would set this to 2×1.25% = 2.5% = 25000 ppm.
   * If this oscillator has 3% tolerance and another chip's has 1%, you would set
   * this to 3% + 1% = 4% = 40000 ppm.
   * @note This is only used if bittime_autogess is true
   */
  uint32_t                  osc_tol;
  /**
   * @brief Estimated propagation delay, in nanoseconds
   * 
   * Internally, this is converted to bit time quanta and always rounded up.
   * 220 might be a good starting point.
   * @note This is only used if bittime_autoguess is true.
   */
  uint16_t                  prop_delay;
  uint16_t                  prescaler;      /**< @brief Only used when bittime_autoguess is false.    */
  uint8_t                   tseg1;          /**< @brief Only used when bittime_autogess is false.     */
  uint8_t                   tseg2;          /**< @brief Only used when bittime_autoguess is false.    */
  uint8_t                   sjw;            /**< @brief Only used when bittime_autoguess is false.    */
  /**
   * @brief Try to determine suitable bit timing parameters automatically
   * 
   * Using the values of bitrate and propdelay, the driver will attempt to
   * pick the best length for the bit time quantum, synchronization jump
   * width (SJW), "phase 1" and "phase 2." (Phase 2 is sometimes referred to as
   * the "information processing time" or IPT, since it is the time after a
   * bit is sampled but before the next bit is transmitted.)
   * 
   * Since the SJW is limited to 4 time quanta, the controller will be most
   * resiliant to clock drift if the time quanta are as large as possible.
   * The driver will try to prescale the system clock as much as possible
   * in order to accomplish this.
   */
  bool                      bittime_autoguess;
} CANConfig;

/**
 * @brief   Structure representing an CAN driver.
 */
struct CANDriver {
  /**
   * @brief   Driver state.
   */
  canstate_t                state;
  /**
   * @brief   Current configuration data.
   */
  const CANConfig           *config;
  /**
   * @brief   Transmission threads queue.
   */
  threads_queue_t           txqueue;
  /**
   * @brief   Receive threads queue.
   */
  threads_queue_t           rxqueue;
#if (CAN_ENFORCE_USE_CALLBACKS == FALSE) || defined (__DOXYGEN__)
  /**
   * @brief   One or more frames become available.
   * @note    After broadcasting this event it will not be broadcasted again
   *          until the received frames queue has been completely emptied. It
   *          is <b>not</b> broadcasted for each received frame. It is
   *          responsibility of the application to empty the queue by
   *          repeatedly invoking @p chReceive() when listening to this event.
   *          This behavior minimizes the interrupt served by the system
   *          because CAN traffic.
   * @note    The flags associated to the listeners will indicate which
   *          receive mailboxes become non-empty.
   */
  event_source_t            rxfull_event;
  /**
   * @brief   One or more transmission mailbox become available.
   * @note    The flags associated to the listeners will indicate which
   *          transmit mailboxes become empty.
   */
  event_source_t            txempty_event;
  /**
   * @brief   A CAN bus error happened.
   * @note    The flags associated to the listeners will indicate the
   *          error(s) that have occurred.
   */
  event_source_t            error_event;
#if (CAN_USE_SLEEP_MODE == TRUE) || defined (__DOXYGEN__)
  /**
   * @brief   Entering sleep state event.
   */
  event_source_t            sleep_event;
  /**
   * @brief   Exiting sleep state event.
   */
  event_source_t            wakeup_event;
#endif
#else /* CAN_ENFORCE_USE_CALLBACKS == TRUE */
  /**
   * @brief   One or more frames become available.
   * @note    After calling this function it will not be called again
   *          until the received frames queue has been completely emptied. It
   *          is <b>not</b> called for each received frame. It is
   *          responsibility of the application to empty the queue by
   *          repeatedly invoking @p chTryReceiveI().
   *          This behavior minimizes the interrupt served by the system
   *          because CAN traffic.
   */
  can_callback_t            rxfull_cb;
  /**
   * @brief   One or more transmission mailbox become available.
   * @note    The flags associated to the callback will indicate which
   *          transmit mailboxes become empty.
   */
  can_callback_t            txempty_cb;
  /**
   * @brief   A CAN bus error happened.
   */
  can_callback_t            error_cb;
#if (CAN_USE_SLEEP_MODE == TRUE) || defined (__DOXYGEN__)
  /**
   * @brief   Exiting sleep state.
   */
  can_callback_t            wakeup_cb;
#endif
#endif
  /* End of the mandatory fields.*/
  /**
   * @brief   CAN module's base address.
   */
  uintptr_t                 can_base;
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (TIVA_CAN_USE_CAN1 == TRUE) && !defined(__DOXYGEN__)
extern CANDriver CAND1;
#endif
#if (TIVA_CAN_USE_CAN2 == TRUE) && !defined(__DOXYGEN__)
extern CANDriver CAND2;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void can_lld_init(void);
  void can_lld_start(CANDriver *canp);
  void can_lld_stop(CANDriver *canp);
  bool can_lld_is_tx_empty(CANDriver *canp, canmbx_t mailbox);
  void can_lld_transmit(CANDriver *canp,
                        canmbx_t mailbox,
                        const CANTxFrame *ctfp);
  bool can_lld_is_rx_nonempty(CANDriver *canp, canmbx_t mailbox);
  void can_lld_receive(CANDriver *canp,
                       canmbx_t mailbox,
                       CANRxFrame *crfp);
  void can_lld_abort(CANDriver *canp,
                     canmbx_t mailbox);
#if CAN_USE_SLEEP_MODE == TRUE
  void can_lld_sleep(CANDriver *canp);
  void can_lld_wakeup(CANDriver *canp);
#endif
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_CAN == TRUE */

#endif /* HAL_CAN_LLD_H */

/** @} */
