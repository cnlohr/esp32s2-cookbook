#ifndef FUNPROG_PORT_H
#define FUNPROG_PORT_H

#if DEBUG_PRINT
#define FUN_PRINTF_DEBUG(format, args...) uprintf(format, ##args)
#else
#define FUN_PRINTF_DEBUG(format, args...)
#endif

#define STATUS_LED_ON
#define STATUS_LED_OFF

#define POWER_SETUP REG_WRITE( IO_MUX_REG(SWCLK_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S ); \
                    REG_WRITE( IO_MUX_REG(SWCLK_PU_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S ); \
                    REG_WRITE( IO_MUX_REG(SWIO_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S ); \
                    REG_WRITE( IO_MUX_REG(SWIO_PU_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );

#define POWER_ON do {GPIO.out_w1ts = pinmaskpower; \
					       GPIO.enable_w1ts = pinmaskpower; \
					       GPIO.enable_w1ts = state.pinmaskD; \
					       GPIO.enable_w1ts = state.pinmaskC; \
					       GPIO.out_w1ts = state.pinmaskD;} while(0)

#define POWER_OFF do {gpio_matrix_out( GPIO_NUM(MULTI2_PIN), 254, 1, 0 ); \
					        GPIO.out_w1tc = state.pinmaskD; \
					        GPIO.out_w1tc = state.pinmaskC; \
					        GPIO.enable_w1ts = state.pinmaskD; \
					        GPIO.enable_w1ts = state.pinmaskC; \
					        GPIO.enable_w1tc = pinmaskpower; \
					        GPIO.out_w1tc = pinmaskpower;} while(0)

#define DELAY_US(n) esp_rom_delay_us(n)
#define DELAY_MS(n) esp_rom_delay_us(n*1000)

#define SLOW_DOWN_PROTOCOL if( state.t1coeff < 12 ) state.t1coeff = 12

// Platform dependatnt attribute to put functions in RAM
#define IRAM IRAM_ATTR

#if defined(MCO_FUNCTION) && MCO_FUNCTION
void EnableMCO( int use_apll, int sdm0, int sdm1, int sdm2, int odiv )
{
	// Output clock on P2.

	// Maximize the drive strength.
	gpio_set_drive_capability( GPIO_NUM(MULTI2_PIN), GPIO_DRIVE_CAP_2 );

	// Use the IO matrix to create the inverse of TX on pin 17.
	gpio_matrix_out( GPIO_NUM(MULTI2_PIN), CLK_I2S_MUX_IDX, 1, 0 );

	periph_module_enable( PERIPH_I2S0_MODULE );

	// Configures APLL = 480 / 4 = 120
	// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );
	// Datasheet recommends that numerator does not exceed 500MHz.
	REG_SET_FIELD( RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD, use_apll ? 0 : 1 );
	REG_SET_FIELD( RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU, use_apll ? 1 : 0 );

	if( use_apll )
	{
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, sdm2);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, sdm0);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, sdm1);
		REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, CLK_LL_APLL_SDM_STOP_VAL_1);
		REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, CLK_LL_APLL_SDM_STOP_VAL_2_REV1);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_OR_OUTPUT_DIV, odiv);
		WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (1<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (2<<I2S_CLKM_DIV_NUM_S) );
	}
	else
	{
		// fI2S = fCLK / ( N + B/A )
		// DIV_NUM = N
		// Note I2S_CLKM_DIV_NUM minimum = 2 by datasheet.  Less than that and it will ignoreeee you.
		WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (2<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (1<<I2S_CLKM_DIV_NUM_S) );  // Minimum reduction, 2:1
	}
}
#endif

#endif
