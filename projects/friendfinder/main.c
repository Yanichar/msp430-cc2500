/*
 * main.c
 */
#include <stdint.h>
#include "device.h"
#include "cc2500.h"

uint8_t rx_callback( uint8_t*, uint8_t );
void delay_ms(uint32_t);

int16_t rssi_threshold = -60;
int16_t rssi_rx = -60;

inline void buzzer_on() {
  LED_PxOUT |= LED2;
}

inline void buzzer_off() {
  LED_PxOUT &= ~LED2;
}

#define RSSI_OFFSET (72)

int16_t rssi_to_dbm(uint8_t rssi)
{
 if( rssi>= 128)
 {
   return (rssi-256)/2 - RSSI_OFFSET;
 }
 else if( rssi < 128)
 {
   return rssi/2 - RSSI_OFFSET;
 }
 else {
   // This shouldn't happen, but if it does, return the minimum
   return -136;
 }

}

void main(void) {

  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

  // Setup oscillator for 8MHz operation
  BCSCTL1 = CALBC1_16MHZ;
  DCOCTL = CALDCO_16MHZ;

  // Wait for changes to take effect
  __delay_cycles(4000);

  // Initialize cc2500 and register callback function to process incoming data
  setup_cc2500(rx_callback);

  // Wait a bit for radio
  delay_ms(10);

  LED_PxOUT &= ~(LED1+LED2);
  LED_PxDIR |= (LED1+LED2);

  // SMCLK/8, up mode, enable interrupt,
  TA0CTL = TASSEL_2 + ID_3+ MC_1 + TAIE + TACLR;
  TACCR0 = 33333; // 16MHz/8/33333 ~ 60Hz

  __bis_SR_register(LPM1_bits +GIE);       // Enter LPM1, enable interrupts

  for(;;) {

    // Compute delay from incoming rssi
    int16_t delay = (rssi_rx-rssi_threshold) * 5;

    // Wait before turning off the buzzer
    delay_ms(delay);

    buzzer_off();

    // Reset to no delay
    rssi_rx = rssi_threshold;

    __bis_SR_register(LPM1_bits + GIE);       // Enter LPM1, enable interrupts
  }
}

// This function is called to process the received packet
uint8_t rx_callback( uint8_t* buffer, uint8_t length )
{
  // If the incoming RSSI is above the threshold, turn on the buffer
  if(rssi_to_dbm(buffer[length]) > rssi_threshold)
  {
    // Convert received RSSI to dbm and store it
    rssi_rx = rssi_to_dbm(buffer[length]);
    buzzer_on();
    return 1; //wake up
  }

  // Don't wake up the processor
  return 0;
}

void delay_ms(uint32_t delay_ms) {
  uint32_t delay;
  for(delay=0; delay < delay_ms; delay++)
  {
    __delay_cycles(16000);
  }
}

// Timer A isr
#pragma vector=TIMERA0_VECTOR
__interrupt void TA0_ISR (void)
{
  static uint8_t counter;

  // Increase virtual timer counter
  counter++;

  if(counter == 60)
  {
    cc2500_tx_packet((uint8_t*)"go!", 3, 0x00);
    // Toggle LED
    LED_PxOUT = LED_PxOUT ^ LED1;

    counter = 0;
  }


  // Clear interrupt flag
  TA0IV &= ~TAIV_TAIFG;

}
