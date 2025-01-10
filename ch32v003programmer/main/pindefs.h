#ifndef _PINDEFS_H
#define _PINDEFS_H

#define BOARD_CNLOHR_REV3
//#define BOARD_CNLOHR
//#define BOARD_S2MINI

#if defined (BOARD_CNLOHR)
/**
 * Pin definitions for the original
 * programmer by CNLohr
*/
#define SWIO_PIN        6
#define SWCLK_PIN       4
#define SWIO_PU_PIN     9
#define SWCLK_PU_PIN    8
#define VDD5V_EN        12
#define VDD3V3_EN       11
#define MULTI2_PIN      2
#define MULTI1_PIN      1
#define WSO_PIN         14

#elif defined(BOARD_S2MINI)
/**
 * Pin definitions for the S2 MINI
 * board
*/
#define SWIO_PIN        6
#define SWCLK_PIN       8
#define SWIO_PU_PIN     7
#define SWCLK_PU_PIN    5
#define VDD5V_EN_PIN    12
#define VDD3V3_EN_PIN   11
#define MULTI2_PIN      2
#define MULTI1_PIN      1
#define LED_PIN         15 // - onboard LED
#elif defined(BOARD_CNLOHR_REV3)
/**
 * Pin definitions for cnlohr's second board.
 * board
*/
#define SWIO_PIN        6
#define SWCLK_PIN       4
#define SWIO_PU_PIN     9
#define SWCLK_PU_PIN    8
#define VDD5V_EN_PIN    3
#define VDD3V3_EN_PIN   5
#define MULTI2_PIN      2
#define MULTI1_PIN      1
#define LED_PIN         15 // - onboard LED
#else 
    #error("Please define board!")
#endif


#define IO_MUX_REG(x) XIO_MUX_REG(x)
#define XIO_MUX_REG(x) IO_MUX_GPIO##x##_REG

#define GPIO_NUM(x) XGPIO_NUM(x)
#define XGPIO_NUM(x) GPIO_NUM_##x


#endif // _PINDEFS_H
