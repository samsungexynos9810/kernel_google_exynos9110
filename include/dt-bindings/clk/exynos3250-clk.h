
/*
 * This header provides constants for Exynos3250 clock controller.
 *
 * The constants defined in this header are being used in dts
 * and exynos3250 clock driver.
 */

#ifndef _DT_BINDINGS_CLK_EXYNOS3250_H
#define _DT_BINDINGS_CLK_EXYNOS3250_H

#define CLK_FIN_PLL		1
#define FOUT_MPLL		2

/* Muxes */
#define CLK_MOUT_MPLL		16
#define CLK_MOUT_MPLL_USER_R	17
#define CLK_MOUT_GDR		18
#define CLK_MOUT_ACLK_100	19
#define CLK_MOUT_ACLK_200	20
#define CLK_MOUT_UART0		21
#define CLK_MOUT_UART1		22
#define CLK_MOUT_UART2		23
#define CLK_MOUT_UART3		24
#define CLK_MOUT_MMC0		25
#define CLK_MOUT_FIMD0		26
#define CLK_MOUT_MIPI0		27
#define CLK_MOUT_ACLK_160	28

/* Dividers */
#define CLK_DIV_MPLL_PRE	64
#define CLK_DIV_ACLK_100	65
#define CLK_DIV_ACLK_200	66
#define CLK_DIV_GPR		67
#define CLK_DIV_GDR		68
#define CLK_DIV_UART0		69
#define CLK_DIV_UART1		70
#define CLK_DIV_UART2		71
#define CLK_DIV_UART3		72
#define CLK_DIV_MMC0		73
#define CLK_DIV_MMC0_PRE	74
#define CLK_DIV_FIMD0		75
#define CLK_DIV_MIPI0		76
#define CLK_DIV_MIPI0_PRE	77
#define CLK_DIV_ACLK_160	78

/* gate and special clocks */
#define CLK_MCT			128
#define CLK_RTC			129
#define SCLK_UART0		130
#define SCLK_UART1		131
#define SCLK_UART2		132
#define SCLK_UART3		133
#define CLK_UART0		134
#define CLK_UART1		135
#define CLK_UART2		136
#define CLK_UART3		137
#define CLK_I2C0		138
#define CLK_I2C1		139
#define CLK_I2C2		140
#define CLK_I2C3		141
#define CLK_I2C4		142
#define CLK_I2C5		143
#define CLK_I2C6		144
#define CLK_I2C7		145
#define CLK_PDMA0		146
#define CLK_PDMA1		147
#define CLK_ACLK_MMC0		148
#define CLK_MMC0		149
#define CLK_ACLK_FIMD0		150
#define CLK_SCLK_FIMD0		151
#define CLK_SCLK_MIPI0		152
#define CLK_FIMD0		153
#define CLK_CLK_DSIM0		154
#define NR_CLKS			256

#endif /*_DT_BINDINGS_CLK_EXYNOS3250_H */
