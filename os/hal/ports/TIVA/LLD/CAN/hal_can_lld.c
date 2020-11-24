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
 * @file    hal_can_lld.c
 * @brief   PLATFORM CAN subsystem low level driver source.
 *
 * @addtogroup CAN
 * @{
 */

#include "hal.h"

#if (HAL_USE_CAN == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   CAN1 driver identifier.
 */
#if (TIVA_CAN_USE_CAN1 == TRUE) || defined(__DOXYGEN__)
CANDriver CAND1;
#endif

/**
 * @brief   CAN1 driver identifier.
 */
#if (TIVA_CAN_USE_CAN2 == TRUE) || defined(__DOXYGEN__)
CANDriver CAND2;
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Set the bitrate for the driver
 * @param[in] confp  Pointer to the CANConfig set with the desired bitrate and prop delay
 * 
 * The Tiva CAN modules have four parameters for configuring bitrate. (See
 * @p CANConfig for details.) The clock prescaler divides the system clock
 * into periods referred to as time quanta (tq). Each bit-time takes several
 * time quanta. The CAN module expects the bus to transition between dominant
 * and recessive (if it is going to do so) during the first time quantum of the
 * bit. This is the synchronization time quantum. Then, a few time quanta are
 * allocated for signal propagation, followed by "phase 1" and "phase 2" (the bus
 * is sampled between phases 1 and 2). Due to clock drift, the module sometimes needs
 * to adjust when it samples the bus based on the actual transition point of the
 * signal. It does this by lengthening phase 1 (when the signal edge occurs after
 * the sync t.q.) or shortening phase 2 (when the signal edge occurs before the sync
 * t.q., during the previous bit's phase 2). It ajusts it by a number of time quanta
 * up to the limit set by the Synchronization Jump Width (SJW). The SJW cannot
 * be greater than phase 2 because if it were, then during a jump the CAN module
 * would have to "change its mind" on the value of a bit after it had already been
 * sampled.
 * 
 * The best timing parameters then, make the SJW as large as possible without exceeding
 * phase 1 or phase 2.
 */
void can_lld_calc_bitrate(CANConfig *confp) {
  assert(confp->bitrate >= 1000);
  assert(confp->prop_delay > 0);
  assert(confp->osc_tol > 0);
  
  if (!confp->bittime_autoguess)
      return;
  
  uint16_t prescaler;  /* 1 to 1024 */
  uint32_t tq_nanos;   /* nanoseconds, theoretical range 1 (25 tq/bit @ 1Mbps) to 250000 (4 tq/bit @ 1kbps) */
  uint16_t prop_tq;    /* propagation delay measured in time quanta */
  uint8_t  phase1_tq;
  uint8_t  phase2_tq;
  uint8_t  sjw;        /* Synchronization Jump Width */
  uint8_t  needed_sjw; /* SJW required to compensate for tolerances */
  bool     match_found = false;
  
  /* Converting from frequency tolerance in ppm to nanoseconds-per-bit tolerance.
   * T = T0 / (1 + Error_frequency) where T0 is the nominal period (1/bitrate) and
   * Error_frequency is the frequency tolerance expressed as a fraction. So the
   * error is T0 - T. Here, we've shuffled the scaling around to avoid roundoff errors.
   */
  uint32_t tolerance_nanos = 1000000000U / (uint64_t)(confp->bitrate) - (1000000000000000U / (uint64_t)(confp->bitrate)) / (1000000U + (uint64_t)(confp->osc_tol));
  
  /* SJW is always at least 1 tq. Phases 1 and 2 must always be greater than
   * SJW. Sync and Propagation each require at least 1 tq. The registers limit
   * the total bit time to 25 tq. Here, we loop through all the possible bit-lengths
   * (as measured in tq) and see whether we can get an integer prescaler value given
   * the system clock speed and desired bitrate. Prescaler max is 1024.
   */
  for (uint8_t try_num_quanta = 4; try_num_quanta <= 25; ++try_num_quanta)
  {
    /* Choose a prescaler that best approximates the target bitrate in case it doesn't divide evenly */
    uint32_t tmp_prescaler = (TIVA_SYSCLK / (uint32_t)(confp->bitrate)) / (uint32_t)try_num_quanta;
    
    for(uint8_t i = 0; i <= 1; ++i)
    {
      tmp_prescaler = tmp_prescaler + i;
    
      if (tmp_prescaler > 1024)
        continue;
      
      uint32_t tmp_tq_nanos = (uint32_t)tmp_prescaler * 1000000U / (TIVA_SYSCLK / 1000U);
      
      /* Nanoseconds per bit of timing error due to imperfect prescaler selection */
      uint32_t mismatch_nanos = abs((int64_t)tmp_tq_nanos * (int64_t)try_num_quanta - (1000000000 / (int32_t)(confp->bitrate)));
      
      uint32_t tmp_needed_sjw = (((int64_t)mismatch_nanos + (int64_t)tolerance_nanos) * 10 + (int64_t)tmp_tq_nanos - 1 /* round up */) / (int64_t)tmp_tq_nanos;
      
      uint16_t tmp_prop_tq = (confp->prop_delay + tmp_tq_nanos - 1 /* round up */) / tmp_tq_nanos;
      int16_t tmp_phase1_tq = ((int32_t)try_num_quanta - 1 /* sync tq */ - tmp_prop_tq + 1 /* round up */) / 2;
      int16_t tmp_phase2_tq = ((int32_t)try_num_quanta - 1 /* sync tq */ - tmp_prop_tq) / 2;
      
      if (tmp_phase2_tq > 0
          && tmp_phase2_tq <= 4
          && tmp_prop_tq + tmp_phase1_tq < 16 /* TSEG1 limited to 16 tq */
          && tmp_needed_sjw <= 4
          && tmp_needed_sjw <= tmp_phase2_tq)
      {
        match_found = true;
        uint8_t tmp_sjw = (tmp_phase2_tq > 4) ? 4 : tmp_phase2_tq;
        
        if ((tmp_sjw - tmp_needed_sjw + 1) * tmp_tq_nanos > (sjw - needed_sjw + 1) * tq_nanos) /* +1 to break the tie when sjw = needed_sjw TODO is this OK? */
        {
          prescaler = tmp_prescaler;
          tq_nanos = tmp_tq_nanos;
          prop_tq = tmp_prop_tq;
          phase1_tq = tmp_phase1_tq;
          phase2_tq = tmp_phase2_tq;
          sjw = tmp_sjw;
          needed_sjw = tmp_needed_sjw;
        }
      }
    }
  }
  
  if (match_found)
  {
    confp->prescaler = prescaler;
    confp->tseg1 = prop_tq + phase1_tq;
    confp->tseg2 = phase2_tq;
    confp->sjw = sjw;
  }
}


/* Filtering */

/* TX "common" ISR (not a real ISR since it takes a CANDriver as an argument) */

/* RX (FIFO 1 and 2?) "common" ISR (not a real ISR since it takes a CANDriver as an argument) */

/* State change (SCE) ISR (not a real ISR since it takes a CANDriver as an argument) */

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (TIVA_CAN_USE_CAN1 == TRUE)
/**
 * @brief CAN1 unified interrupt handler
 * 
 * Unlike some STM32 chips, the Tiva chips do not have separate IRQs for
 * different CAN events.
 * @isr
 */
OSAL_IRQ_HANDLER(TIVA_CAN0_HANDLER) {
    OSAL_IRQ_PROLOGUE();
    
    
    OSAL_IRQ_EPILOGUE();
}
#endif /* TIVA_CAN_USE_CAN1 == TRUE */

#if (TIVA_CAN_USE_CAN2 == TRUE)
/**
 * @brief CAN1 unified interrupt handler
 * 
 * Unlike some STM32 chips, the Tiva chips do not have separate IRQs for
 * different CAN events.
 * @isr
 */
OSAL_IRQ_HANDLER(TIVA_CAN1_HANDLER) {
    OSAL_IRQ_PROLOGUE();
    
    
    OSAL_IRQ_EPILOGUE();
}
#endif /* TIVA_CAN_USE_CAN2 == TRUE */

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level CAN driver initialization.
 *
 * @notapi
 */
void can_lld_init(void) {
#if TIVA_CAN_USE_CAN1 == TRUE
  /* Driver initialization.*/
  canObjectInit(&CAND1);
  CAND1.can_base = CAN0_BASE;
  nvicEnableVector(TIVA_CAN0_NUMBER, TIVA_CAN1_IRQ_PRIORITY);
#endif
#if TIVA_CAN_USE_CAN2 == TRUE
  canObjectInit(&CAND2);
  CAND2.can_base = CAN1_BASE;
  nvicEnableVector(TIVA_CAN1_NUMBER, TIVA_CAN2_IRQ_PRIORITY);
#endif
  
  /* TODO: Set a sane default filtering here */
}

/**
 * @brief   Configures and activates the CAN peripheral.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 *
 * @notapi
 */
void can_lld_start(CANDriver *canp) {
  /* Active clocks and wait for modules to become ready.*/
#if TIVA_CAN_USE_CAN1 == TRUE
  if (&CAND1 == canp) {
    HWREG(SYSCTL_RCGCCAN) |= SYSCTL_RCGCCAN_R0;
    while ( !(HWREG(SYSCTL_PRCAN) & SYSCTL_PRCAN_R0))
      ;
  }
#endif
#if TIVA_CAN_USE_CAN2 == TRUE
  if (&CAND2 == canp) {
    HWREG(SYSCTL_RCGCCAN) |= SYSCTL_RCGCCAN_R1;
    while ( !(HWREG(SYSCTL_PRCAN) & SYSCTL_PRCAN_R1))
      ;
  }
#endif
  /* Configures the peripheral.*/

}

/**
 * @brief   Deactivates the CAN peripheral.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 *
 * @notapi
 */
void can_lld_stop(CANDriver *canp) {

  if (canp->state == CAN_READY) {
    /* Resets the peripheral.*/

    /* Disables the peripheral.*/
#if TIVA_CAN_USE_CAN1 == TRUE
    if (&CAND1 == canp) {

    }
#endif
  }
}

/**
 * @brief   Determines whether a frame can be transmitted.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 * @param[in] mailbox   mailbox number, @p CAN_ANY_MAILBOX for any mailbox
 *
 * @return              The queue space availability.
 * @retval false        no space in the transmit queue.
 * @retval true         transmit slot available.
 *
 * @notapi
 */
bool can_lld_is_tx_empty(CANDriver *canp, canmbx_t mailbox) {

  (void)canp;

  switch (mailbox) {
  case CAN_ANY_MAILBOX:
    return false;
  case 1:
    return false;
  case 2:
    return false;
  case 3:
    return false;
  default:
    return false;
  }
}

/**
 * @brief   Inserts a frame into the transmit queue.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 * @param[in] ctfp      pointer to the CAN frame to be transmitted
 * @param[in] mailbox   mailbox number,  @p CAN_ANY_MAILBOX for any mailbox
 *
 * @notapi
 */
void can_lld_transmit(CANDriver *canp,
                      canmbx_t mailbox,
                      const CANTxFrame *ctfp) {

  (void)canp;
  (void)mailbox;
  (void)ctfp;

}

/**
 * @brief   Determines whether a frame has been received.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 * @param[in] mailbox   mailbox number, @p CAN_ANY_MAILBOX for any mailbox
 *
 * @return              The queue space availability.
 * @retval false        no space in the transmit queue.
 * @retval true         transmit slot available.
 *
 * @notapi
 */
bool can_lld_is_rx_nonempty(CANDriver *canp, canmbx_t mailbox) {

  (void)canp;
  (void)mailbox;

  switch (mailbox) {
  case CAN_ANY_MAILBOX:
    return false;
  case 1:
    return false;
  case 2:
    return false;
  default:
    return false;
  }
}

/**
 * @brief   Receives a frame from the input queue.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 * @param[in] mailbox   mailbox number, @p CAN_ANY_MAILBOX for any mailbox
 * @param[out] crfp     pointer to the buffer where the CAN frame is copied
 *
 * @notapi
 */
void can_lld_receive(CANDriver *canp,
                     canmbx_t mailbox,
                     CANRxFrame *crfp) {

  (void)canp;
  (void)mailbox;
  (void)crfp;

}

/**
 * @brief   Tries to abort an ongoing transmission.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 * @param[in] mailbox   mailbox number
 *
 * @notapi
 */
void can_lld_abort(CANDriver *canp,
                   canmbx_t mailbox) {

  (void)canp;
  (void)mailbox;
}

#if (CAN_USE_SLEEP_MODE == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Enters the sleep mode.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 *
 * @notapi
 */
void can_lld_sleep(CANDriver *canp) {

  (void)canp;

}

/**
 * @brief   Enforces leaving the sleep mode.
 *
 * @param[in] canp      pointer to the @p CANDriver object
 *
 * @notapi
 */
void can_lld_wakeup(CANDriver *canp) {

  (void)canp;

}
#endif /* CAN_USE_SLEEP_MOD == TRUEE */

#endif /* HAL_USE_CAN == TRUE */

/** @} */
