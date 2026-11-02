#ifndef APP_IRQ_HANDLERS_H
#define APP_IRQ_HANDLERS_H

/* RH850 EIINT CHANNEL indexes,not RL78 vector byte addresses.
 * Source: R7F701684 SC r_cg_intvector.c and hardware manual interrupt table.
 * 23=INTRCANGRECC0 receive FIFO;84=INTOSTM0 periodic tick.
 * Add X(channel,global_ISR_name) for an App peripheral. Define that ISR with
 * #pragma interrupt name(enable=false,fpu=true,callt=false),without channel=.
 * The generator maps it in this App's complete512-entry table. A Boot image
 * is not regenerated when App mappings change. Names may match another
 * independently linked image;duplicate definitions inside one image fail.
 */
#define APP_IRQ_HANDLERS(X)                                                                        \
    X(23U, Device_CanRxIsr)                                                                        \
    X(84U, r_Config_OSTM0_interrupt)

#endif
