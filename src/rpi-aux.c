#include <stdio.h>
#include "rpi-aux.h"
#include "rpi-base.h"
#include "rpi-gpio.h"
#include "info.h"
#include "startup.h"
#include "stdlib.h"

#ifdef INCLUDE_DEBUGGER
#include "debugger/debugger.h"
#endif

/* Define the system clock frequency in MHz for the baud rate calculation.
 This is clearly defined on the BCM2835 datasheet errata page:
 http://elinux.org/BCM2835_datasheet_errata */
#define FALLBACK_SYS_FREQ    250000000

// #define USE_IRQ

#define TX_BUFFER_SIZE 65536  // Must be a power of 2

static aux_t* auxillary = (aux_t*) AUX_BASE;

aux_t* RPI_GetAux(void)
{
  return auxillary;
}

#if defined(USE_IRQ)

#include "rpi-interrupts.h"

// Note, at the point the MiniUART is initialized, low vectors are in use
#define IRQ_VECTOR 0x38

static char *tx_buffer;
static volatile int tx_head;
static volatile int tx_tail;

static void __attribute__((interrupt("IRQ"))) RPI_AuxMiniUartIRQHandler() {

  while (1) {

    int iir = auxillary->MU_IIR;

    if (iir & AUX_MUIIR_INT_NOT_PENDING) {
      /* No more interrupts */
      break;
    }

    /* Handle RxReady interrupt */
    if (iir & AUX_MUIIR_INT_IS_RX) {
#ifdef INCLUDE_DEBUGGER
      /* Forward all received characters to the debugger */
      debugger_rx_char(auxillary->MU_IO & 0xFF);
#else
      /* Else just exho characters */
      RPI_AuxMiniUartWrite(auxillary->MU_IO & 0xFF);
#endif
    }

    /* Handle TxEmpty interrupt */
    if (iir & AUX_MUIIR_INT_IS_TX) {
      if (tx_tail != tx_head) {
        /* Transmit the character */
        tx_tail = (tx_tail + 1) & (TX_BUFFER_SIZE - 1);
        auxillary->MU_IO = tx_buffer[tx_tail];
      } else {
        /* Disable TxEmpty interrupt */
        auxillary->MU_IER &= ~AUX_MUIER_TX_INT;
      }
    }
  }
}
#endif

void RPI_AuxMiniUartInit(int baud, int bits)
{
  volatile int i;

  int sys_freq = get_clock_rate(CORE_CLK_ID);

  if (!sys_freq) {
     sys_freq = FALLBACK_SYS_FREQ;
  }

  /* As this is a mini uart the configuration is complete! Now just
   enable the uart. Note from the documentation in section 2.1.1 of
   the ARM peripherals manual:

   If the enable bits are clear you will have no access to a
   peripheral. You can not even read or write the registers */
  auxillary->ENABLES = AUX_ENA_MINIUART;

  /* Disable flow control,enable transmitter and receiver! */
  auxillary->MU_CNTL = 0;

  /* Decide between seven or eight-bit mode */
  if (bits == 8)
    auxillary->MU_LCR = AUX_MULCR_8BIT_MODE;
  else
    auxillary->MU_LCR = 0;

  auxillary->MU_MCR = 0;

  /* Disable all interrupts from MU and clear the fifos */
  auxillary->MU_IER = 0;
  auxillary->MU_IIR = 0xC6;

  /* Transposed calculation from Section 2.2.1 of the ARM peripherals manual */
  auxillary->MU_BAUD = ( sys_freq / (8 * baud)) - 1;

#ifdef USE_IRQ
  tx_buffer = malloc(TX_BUFFER_SIZE);
  tx_head = tx_tail = 0;
  *((uint32_t *) IRQ_VECTOR) = (uint32_t) RPI_AuxMiniUartIRQHandler;
  RPI_GetIrqController()->Enable_IRQs_1 = (1 << 29);
  auxillary->MU_IER |= AUX_MUIER_RX_INT;  
#endif

  /* Setup GPIO 14 and 15 as alternative function 5 which is
   UART 1 TXD/RXD. These need to be set before enabling the UART */
  RPI_SetGpioPinFunction(RPI_GPIO14, FS_ALT5);
  RPI_SetGpioPinFunction(RPI_GPIO15, FS_ALT5);

  // Enable weak pullups
  RPI_GpioBase->GPPUD = 2;
  // Note: the delay values are important, with 150 the receiver did not work!
  for (i = 0; i < 1000; i++)
  {
  }
  RPI_GpioBase->GPPUDCLK0 = (1 << 14) | (1 << 15);
  // Note: the delay values are important, with 150 the receiver did not work!
  for (i = 0; i < 1000; i++)
  {
  }
  RPI_GpioBase->GPPUDCLK0 = 0;

  /* Disable flow control,enable transmitter and receiver! */
  auxillary->MU_CNTL = AUX_MUCNTL_TX_ENABLE | AUX_MUCNTL_RX_ENABLE;
}

void RPI_AuxMiniUartWrite(char c)
{
#ifdef USE_IRQ
  int tmp_head = (tx_head + 1) & (TX_BUFFER_SIZE - 1);

  /* Test if the buffer is full */
  if (tmp_head == tx_tail) {
     int cpsr = _get_cpsr();
     if (cpsr & 0x80) {
        /* IRQ disabled: drop the character to avoid deadlock */
        return;
     } else {
        /* IRQ enabled: wait for space in buffer */
        while (tmp_head == tx_tail) {
        }
     }
  }
  /* Buffer the character */
  tx_buffer[tmp_head] = c;
  tx_head = tmp_head;

  /* Enable TxEmpty interrupt */
  auxillary->MU_IER |= AUX_MUIER_TX_INT;
#else
  /* Wait until the UART has an empty space in the FIFO */
  while ((auxillary->MU_LSR & AUX_MULSR_TX_EMPTY) == 0)
  {
  }
  /* Write the character to the FIFO for transmission */
  auxillary->MU_IO = c;
#endif
}

extern void RPI_EnableUart(char* pMessage)
{
  RPI_AuxMiniUartInit(115200, 8);          // Initialise the UART

  printf(pMessage);
}
