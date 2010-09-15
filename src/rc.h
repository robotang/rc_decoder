#ifndef RC_H
#define RC_H

/* System Control Module Definitions */
#define PADCONF_IEN				(1 << 8) /* Input enable */
#define PADCONF_IDIS				(0 << 8) /* Input disable */
#define PADCONF_PULL_UP				(1 << 4) /* Pull type up */
#define PADCONF_PULL_DOWN			(0 << 4) /* Pull type down */
#define PADCONF_PULL_EN				(1 << 3) /* Enable pull up or down resistor */
#define PADCONF_PULL_DIS			(0 << 3) /* Disable pull up or down resistor */
#define PADCONF_GPIO_MODE			4

#define OMAP34XX_PADCONF_START			0x48002030
#define OMAP34XX_PADCONF_SIZE			0x000005CC

/* General Purpose Interface Definitions */
#define OMAP34XX_GPIO1_REG_BASE			0x48310000 /* GPIO[  0: 31] */
#define OMAP34XX_GPIO2_REG_BASE			0x49050000 /* GPIO[ 32: 63] */
#define OMAP34XX_GPIO3_REG_BASE			0x49052000 /* GPIO[ 64: 95] */
#define OMAP34XX_GPIO4_REG_BASE			0x49054000 /* GPIO[ 96:127] */
#define OMAP34XX_GPIO5_REG_BASE			0x49056000 /* GPIO[128:159] */
#define OMAP34XX_GPIO6_REG_BASE			0x49058000 /* GPIO[160:191] */
#define OMAP34XX_GPIO_REG_SIZE			1024

#define GPIO_IRQENABLE1_REG_OFFSET		0x0000001C
#define GPIO_IRQENABLE2_REG_OFFSET		0x0000002C
#define GPIO_OE_REG_OFFSET			0x00000034
#define GPIO_DATAIN_REG_OFFSET			0x00000038
#define GPIO_DATAOUT_REG_OFFSET			0x0000003C
#define GPIO_RISINGDETECT_REG_OFFSET		0x00000048
#define GPIO_FALLINGDETECT_REG_OFFSET		0x0000004C
#define GPIO_CLEARIRQENABLE1_REG_OFFSET		0x00000060
#define GPIO_CLEARIRQENABLE2_REG_OFFSET		0x00000070
#define GPIO_SETIRQENABLE1_REG_OFFSET		0x00000064
#define GPIO_SETIRQENABLE2_REG_OFFSET		0x00000074
#define GPIO_CLEARDATAOUT_REG_OFFSET		0x00000090
#define GPIO_SETDATAOUT_REG_OFFSET		0x00000094

/* General Purpose Timer Definitions */
#define CLK_32K_FREQ				32768
#define CLK_SYS_FREQ				13000000 /* 13MHz */

#define GPTIMER8				0x4903E000 /* Defaults to CLK_SYS_FREQ (13MHz) - is this always true? */
#define GPTIMER9				0x49040000 /* Defaults to CLK_SYS_FREQ (13MHz) - is this always true? */
#define GPTIMER10 				0x48086000 /* Defaults to CLK_32K_FREQ - is this always true? */
#define GPTIMER11				0x48088000 /* Defaults to CLK_32K_FREQ - is this always true? */

#define GPT_TIOCP_CFG 				0x00000010
#define GPT_TISTAT    				0x00000014
#define GPT_TISR      				0x00000018
#define GPT_TIER      				0x0000001C
#define GPT_TWER      				0x00000020
#define GPT_TCLR      				0x00000024
#define GPT_TCRR      				0x00000028
#define GPT_TLDR     				0x0000002C
#define GPT_TTGR      				0x00000030
#define GPT_TWPS			    	0x00000034
#define GPT_TMAR 				0x00000038
#define GPT_TCAR1     				0x0000003C
#define GPT_TSICR     				0x00000040
#define GPT_TCAR2     				0x00000044
#define GPT_TPIR      				0x00000048
#define GPT_TNIR      				0x0000004C
#define GPT_TCVR      				0x00000050
#define GPT_TOCR      				0x00000054
#define GPT_TOWR      				0x00000058

#define GPT_REGS_PAGE_SIZE      		4096

#define GPT_TCLR_ST_START     			(1 << 0) 
#define GPT_TCLR_ST_STOP     			(0 << 0)
#define GPT_TCLR_PRESCALE_EN			(1 << 5)
#define GPT_TCLR_PRESCALE_DIS			(0 << 5)
#define GPT_TCLR_PS_DIV1			GPT_TCLR_PS_DIV1
#define GPT_TCLR_PS_DIV2			(0 << 2) 
#define GPT_TCLR_PS_DIV4			(1 << 2)
#define GPT_TCLR_PS_DIV8			(2 << 2)
#define GPT_TCLR_PS_DIV16			(3 << 2)
#define GPT_TCLR_PS_DIV32 			(4 << 2)

#endif

