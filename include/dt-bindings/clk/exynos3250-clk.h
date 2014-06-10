
/*
 * This header provides constants for Exynos3250 clock controller.
 *
 * The constants defined in this header are being used in dts
 * and exynos3250 clock driver.
 */

#ifndef _DT_BINDINGS_CLK_EXYNOS3250_H
#define _DT_BINDINGS_CLK_EXYNOS3250_H

#define FIN_PLL			1
#define FOUT_MPLL		2
#define SCLK_MPLL		3
#define HALF_MPLL		4
#define MPLL_PRE_DIV		5
#define MOUT_USER_BUS_R		40
#define MUX_GDR			41
#define MOUT_ACLK100		42
#define ACLK100			43
#define ACLK200			44
#define MOUT_ACLK200		45
#define DCLK_GPR		60
#define DCLK_GDR		61
#define CLK_MCT			80
#define MOUT_UART0		81
#define MOUT_UART1		82
#define MOUT_UART2		83
#define MOUT_UART3		84
#define CLK_RTC			85
#define SCLK_UART0		200
#define SCLK_UART1		201
#define SCLK_UART2		202
#define SCLK_UART3		203
#define DOUT_UART0		250
#define DOUT_UART1		251
#define DOUT_UART2		252
#define DOUT_UART3		253
#define GATE_UART0		300
#define GATE_UART1		301
#define GATE_UART2		302
#define GATE_UART3		303
#define GATE_I2C0		304
#define GATE_I2C1		305
#define GATE_I2C2		306
#define GATE_I2C3		307
#define GATE_I2C4		308
#define GATE_I2C5		309
#define GATE_I2C6		310
#define GATE_I2C7		311
#define CLK_PDMA0		312
#define CLK_PDMA1		313
#define ACLK_MMC0		400
#define GATE_MMC0		401
#define DOUT_MMC_A		402
#define DOUT_MMC_B		403
#define MOUT_MMC0		404
#define MOUT_FIMD0		405
#define MOUT_MIPI0		406
#define MOUT_ACLK_160		407
#define DOUT_FIMD0		408
#define DOUT_MIPI0		409
#define DOUT_MIPI0_PRE		410
#define DOUT_ACLK_160		411
#define GATE_ACLK_FIMD0		412
#define GATE_SCLK_FIMD0		413
#define GATE_SCLK_MIPI0		414
#define GATE_CLK_FIMD0		415
#define GATE_CLK_DSIM0		416
#define NR_CLKS			417

#endif /*_DT_BINDINGS_CLK_EXYNOS3250_H */
