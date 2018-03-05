/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

/* Since we *know* there will be 2 processes, stemming from the 2 user
 * programs, we can
 *
 * - allocate a fixed-size process table (of PCBs), and then maintain
 *   an index into it for the currently executing process,
 * - employ a fixed-case of round-robin scheduling: no more processes
 *   can be created, and neither is able to terminate.
 */

pcb_t pcb[ 2 ]; int executing = 0;

void scheduler( ctx_t* ctx ) {
  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'S', true );
  PL011_putc( UART0, 'C', true );
  PL011_putc( UART0, 'H', true );

  if     ( 0 == executing ) {

    memcpy( &pcb[ 0 ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_1
    pcb[ 0 ].status = STATUS_READY;                // update   P_1 status
    memcpy( ctx, &pcb[ 1 ].ctx, sizeof( ctx_t ) ); // restore  P_2
    pcb[ 1 ].status = STATUS_EXECUTING;            // update   P_2 status
    executing = 1;                                 // update   index => P_2
  }
  else if( 1 == executing ) {

    memcpy( &pcb[ 1 ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_2
    pcb[ 1 ].status = STATUS_READY;                // update   P_2 status
    memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) ); // restore  P_1
    pcb[ 0 ].status = STATUS_EXECUTING;            // update   P_1 status
    executing = 0;                                 // update   index => P_1
  }

  return;
}

extern void main_P3();
extern uint32_t tos_P3;
extern void main_P4();
extern uint32_t tos_P4;

void hilevel_handler_rst( ctx_t* ctx              ) {

  /* Initialise PCBs representing processes stemming from execution of
   * the two user programs.  Note in each case that
   *
   * - the CPSR value of 0x50 means the processor is switched into USR
   *   mode, with IRQ interrupts enabled, and
   * - the PC and SP values matche the entry point and top of stack.
   */
   PL011_putc( UART0, ' ', true );
   PL011_putc( UART0, 'R', true );
   PL011_putc( UART0, 'S', true );
   PL011_putc( UART0, 'T', true );

  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].status   = STATUS_READY;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &(tos_P3)  );

  memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
  pcb[ 1 ].pid      = 2;
  pcb[ 1 ].status   = STATUS_READY;
  pcb[ 1 ].ctx.cpsr = 0x50;
  pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
  pcb[ 1 ].ctx.sp   = ( uint32_t )( &(tos_P4)  );

  /* Once the PCBs are initialised, we (arbitrarily) select one to be
   * restored (i.e., executed) when the function then returns.
   */

  memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
  pcb[ 0 ].status = STATUS_EXECUTING;
  executing = 0;

    TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
    TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
    TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
    TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
    TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

    GICC0->PMR          = 0x000000F0; // unmask all            interrupts
    GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
    GICC0->CTLR         = 0x00000001; // enable GIC interface
    GICD0->CTLR         = 0x00000001; // enable GIC distributor

    int_enable_irq();

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;
  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'I', true );
  PL011_putc( UART0, 'R', true );
  PL011_putc( UART0, 'Q', true );
// Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    TIMER0->Timer1IntClr = 0x01;
    scheduler(ctx);
  }

// Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;
  return;
}

// lolevel irq:
// bic   r1, r1, 0xFF000000
// ldr   r1, [le, #-4]
// @ stmfd sp!, { r0-r3, ip, lr }  @ save    caller-save registers
void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identified encoded as an immediate operand in the
   * instruction,
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call,
   * - write any return value back to preserved usr mode registers.
   */
  // PL011_putc( UART0, ' ', true );
  // PL011_putc( UART0, 'S', true );
  // PL011_putc( UART0, 'V', true );
  // PL011_putc( UART0, 'C', true );
  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      // scheduler( ctx );
      // PL011_putc( UART0, 'A', true );
      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      // PL011_putc( UART0, 'B', true );
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      // PL011_putc( UART0, 'C', true );
      break;
    }
  }

  return;
}