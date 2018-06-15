/*
 * Memory Setup stuff - taken from blob memsetup.S
 *
 * Copyright (C) 1999 2000 2001 Erik Mouw (J.A.K.Mouw@its.tudelft.nl) and
 *                     Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * Modified for the Samsung SMDK2410 by
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#include <config.h>
#include <version.h>

#include <s5pc110.h>
#include "smdkc110_val.h"

_TEXT_BASE:
	.word	TEXT_BASE
// lowlevel_init函数都是底层的代码
	.globl lowlevel_init
lowlevel_init:
	// 把lr进行压栈，因为进来的时候lr里保存的是函数的返回地址，
	// 在这个函数下面还会调用其他函数，所以要对lowlevel_init函数的地址进行压栈
	// 这样执行完lowlevel_init函数后，可以返回回去
	// 如果不对lr进行压栈，下面调用其他函数时，lowlevel_init函数存在lr中的地址会被覆盖
	// 也就是会丢
	push	{lr}	// push是进栈

	/* check reset status 检查复位状态 与主线启动代码无关*/
	// 复杂CPU允许多种复位情况。譬如直接冷上电、热启动、睡眠(低功耗)状态下的唤醒等
	// 这些情况都属于复位。所以我们要在复位代码中检测复位状态来判断到底是哪种情况
	// 判断哪种复位的意义在于: 冷上电时DDR是需要初始化才能用的
	// 而热启动或者低功耗状态下的复位则不需要再次初始化DDR。
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE+RST_STAT_OFFSET)	// 把寄存器的地址写入到r0中
	ldr	r1, [r0]
	bic	r1, r1, #0xfff6ffff
	cmp	r1, #0x10000
	beq	wakeup_reset_pre
	cmp	r1, #0x80000
	beq	wakeup_reset_from_didle

	/* IO Retention release IO恢复，与主线启动代码无关*/
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE + OTHERS_OFFSET)	// 把寄存器的地址写入到r0中
	ldr	r1, [r0]
	ldr	r2, =IO_RET_REL
	orr	r1, r1, r2
	str	r1, [r0]

	/* Disable Watchdog 向WTCON的bit5写入0即可*/
	ldr	r0, =ELFIN_WATCHDOG_BASE	/* 0xE2700000 	把寄存器的地址写入到r0中*/
	mov	r1, #0		// 往r1中写0
	str	r1, [r0]	// 把r1中的值写到r0所指向的寄存器中

	/* SRAM(2MB) init for SMDKC110 	开发板外部外接了SRAM，所以要对外部SRAM初始化	与主线启动代码无关*/
	/* GPJ1 SROM_ADDR_16to21 */
	ldr	r0, =ELFIN_GPIO_BASE
	
	ldr	r1, [r0, #GPJ1CON_OFFSET]
	bic	r1, r1, #0xFFFFFF
	ldr	r2, =0x444444
	orr	r1, r1, r2
	str	r1, [r0, #GPJ1CON_OFFSET]

	ldr	r1, [r0, #GPJ1PUD_OFFSET]
	ldr	r2, =0x3ff
	bic	r1, r1, r2
	str	r1, [r0, #GPJ1PUD_OFFSET]

	/* GPJ4 SROM_ADDR_16to21 	开发板外部外接了SROM，所以要对外部SROM初始化	与主线启动代码无关*/
	ldr	r1, [r0, #GPJ4CON_OFFSET]
	bic	r1, r1, #(0xf<<16)
	ldr	r2, =(0x4<<16)
	orr	r1, r1, r2
	str	r1, [r0, #GPJ4CON_OFFSET]

	ldr	r1, [r0, #GPJ4PUD_OFFSET]
	ldr	r2, =(0x3<<8)
	bic	r1, r1, r2
	str	r1, [r0, #GPJ4PUD_OFFSET]


	/* CS0 - 16bit sram, enable nBE, Byte base address */
	ldr	r0, =ELFIN_SROM_BASE	/* 0xE8000000 */
	mov	r1, #0x1
	str	r1, [r0]

	/* PS_HOLD pin(GPH0_0) set to high；PS_HOLD是开发板供电锁存   */
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE + PS_HOLD_CONTROL_OFFSET)
	ldr	r1, [r0]		// 把r0指向的寄存器中的数写到r1中
	orr	r1, r1, #0x300	// r1与0x300取或
	orr	r1, r1, #0x1	// r1与1取或	
	str	r1, [r0]		// 把r1得到的值写到r0所指向的寄存器中

// 在这之前，lowlevel_init函数中并没有做太多有意义的事情(除了关看门狗、开发板供电锁存外)
// 从这以后，lowlevel_init函数才开始进行有意义的操作
	/* when we already run in ram, we don't need to relocate U-Boot.
	 * and actually, memory controller must be configured before U-Boot
	 * is running in ram.
	 */		
	// 130-141这几行代码判断当前代码执行的位置是在iSAM还是在DDR中
	// 为什么要做这个判定? 
	// 原因1: BL1(uboot的前一部分)在SRAM中有一份，在DDR中也有一份，
	// 因此如果冷启动那么当前代码应该是在SRAM中运行的BL1，
	// 如果是低功耗状态的复位这时候应该就是在DDR中运行的。
	// 原因2: 我们判定当前运行代码的地址有用的，可以指导后面代码的运行
	// 譬如在lowlevel_init.S中判定当前代码的运行地址，就是为了确定要不要
	// 执行时钟初始化和初始化DDR的代码。如果是当前代码是在SRAM中，说明
	// 冷启动，那么时钟和DDR都需要初始化；如果当前代码是在DDR中，那么说明
	// 是热启动，那么说明时钟和DDR都是不用再次初始化。
	ldr	r0, =0xff000fff
	// 意义: 将pc的值中的某些bit位清0，剩下一些特殊的bit位赋值给r1，相当于r1 = pc & ~(0xff000fff)
	bic	r1, pc, r0		/* r0 <- current base addr of code ；pc是当前代码的地址*/
	// 把链接地址加载到r2中；链接地址(虚拟地址)，也就是DDR中的地址，这也就说明了如果相等DDR已经初始化完了
	ldr	r2, _TEXT_BASE		/* r1 <- original base addr in ram ；*/
	// 将r2的相应位清0剩下特定位
	bic	r2, r2, r0		/* r0 <- current base addr of code */
	// 对比的方法是pc将自己走过的那些地址去掉，然后与链接地址进行对比，与裸机中的有些不同
	cmp     r1, r2                  /* compare r0, r1                  */
	// 如果r1和r2相等，当前代码是DDR中，热启动，不需要初始化时钟和DDR
	// 如果r1和r2不相等，当前代码在SRAM中，冷启动，需要初始化时钟和DDR
	beq     1f			/* r0 == r1 then skip sdram init   */

	/* init system clock */
	bl system_clock_init	// 初始化时钟

	/* Memory initialize */
	bl mem_ctrl_asm_init	// 初始化DDR
	
1:
	/* for UART */
	bl uart_asm_init
	// trust zone初始化
	bl tzpc_init			// 不需要管理，不管即可

#if defined(CONFIG_ONENAND)
	bl onenandcon_init
#endif

#if defined(CONFIG_NAND)
	/* simple init for NAND */
	bl nand_asm_init
#endif

	/* check reset status  检查复位状态*/
	
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE+RST_STAT_OFFSET)
	ldr	r1, [r0]
	bic	r1, r1, #0xfffeffff
	cmp	r1, #0x10000
	beq	wakeup_reset_pre

	/* ABB disable */
	ldr	r0, =0xE010C300
	orr	r1, r1, #(0x1<<23)
	str	r1, [r0]

	/* Print 'K' 打印K，也是为了做串口调试，*/
	// lowlevel_init执行完如果没错那么就会串口打印"OK"字样。这就是uboot中看到的最早的输出信息
	ldr	r0, =ELFIN_UART_CONSOLE_BASE
	ldr	r1, =0x4b4b4b4b
	str	r1, [r0, #UTXH_OFFSET]		// 返回前通过串口打印'K'
	// pop是出栈
	pop	{pc}

wakeup_reset_from_didle:
	/* Wait when APLL is locked */
	ldr	r0, =ELFIN_CLOCK_POWER_BASE
lockloop:
	ldr	r1, [r0, #APLL_CON0_OFFSET]
	and	r1, r1, #(1<<29)
	cmp	r1, #(1<<29)
	bne 	lockloop
	beq	exit_wakeup

wakeup_reset_pre:
	mrc	p15, 0, r1, c1, c0, 1	@Read CP15 Auxiliary control register
	and	r1, r1, #0x80000000	@Check L2RD is disable or not
	cmp	r1, #0x80000000		
	bne	wakeup_reset		@if L2RD is not disable jump to wakeup_reset 
	
	bl	disable_l2cache
	bl	v7_flush_dcache_all
	/* L2 cache enable at sleep.S of kernel
	 * bl	enable_l2cache 
	 */

wakeup_reset:
	/* init system clock */
	bl system_clock_init
	bl mem_ctrl_asm_init
	bl tzpc_init
#if defined(CONFIG_ONENAND)
	bl onenandcon_init
#endif
#if defined(CONFIG_NAND)
	bl nand_asm_init
#endif

exit_wakeup:
	/*Load return address and jump to kernel*/
	ldr	r0, =(INF_REG_BASE+INF_REG0_OFFSET)
	ldr	r1, [r0]	/* r1 = physical address of s5pc110_cpu_resume function*/

	mov	pc, r1		/*Jump to kernel */
	nop
	nop

/*
 * system_clock_init: Initialize core clock and bus clock.
 * void system_clock_init(void)
 */
system_clock_init:
	// 下面的过程就是初始化时钟
	ldr	r0, =ELFIN_CLOCK_POWER_BASE	@0xe0100000
// 1、我们这里主要是暂时不适用APLL(Control MUXAPLL)
	/* Set Mux to FIN */
	ldr	r1, =0x0
	str	r1, [r0, #CLK_SRC0_OFFSET]		// 访问CLK_SRC0寄存器，并且全部写0
// 2、设置APLL锁定时间，设置PLL后，时钟从Fin提升到目标频率时，需要一定时间，即锁定时间
	ldr	r1,	=APLL_LOCKTIME_VAL
	str	r1,	[r0, #APLL_LOCK_OFFSET]

	/********lxg added*********************/
	ldr	r0, =ELFIN_CLOCK_POWER_BASE	@0xe0100000
// 2、设置MPLL锁定时间
	ldr	r1,	=MPLL_LOCKTIME_VAL
	str	r1,	[r0, #MPLL_LOCK_OFFSET]
	/********end*********************/

	/* Disable PLL */
#if defined(CONFIG_CHECK_MPLL_LOCK)
retryloop:
#endif
	ldr	r1, =0x0
	str	r1, [r0, #APLL_CON0_OFFSET]
	ldr	r1, =0x0
	str	r1, [r0, #MPLL_CON_OFFSET]
// 258-259行将APLL_CON0寄存器和MPLL_CON寄存器各个位都置位0
	ldr	r1, =0x0
	str	r1, [r0, #MPLL_CON_OFFSET]
// 3、261-263行，设置CLK_DIV0寄存器分频
	ldr   	r1, [r0, #CLK_DIV0_OFFSET]
	ldr	r2, =CLK_DIV0_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV0_VAL	// 将CLK_DIV0_VAL的值写到CLK_DIV0寄存器中，从而计算PCLK_PSYS/HCLK_PSYS/PCLK_DSYS/HCLK_DSYS/PCLK_MSYS/HCLK_MSYS/SCLKA2M/ARMCLK的值
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV0_OFFSET]
///4、269-280行设置APLL/MPLL/VPLL/EPLL，主要设置PLL的倍频系数，决定由输入端24MHz的原始频率可以得到多大的输出频率
	ldr	r1, =APLL_VAL		// 设置方法参考数据手册时钟部分(372)
	str	r1, [r0, #APLL_CON0_OFFSET]

	ldr	r1, =MPLL_VAL
	str	r1, [r0, #MPLL_CON_OFFSET]

	ldr	r1, =VPLL_VAL
	str	r1, [r0, #VPLL_CON_OFFSET]

	/*******lxg added***********************/
	ldr	r1, =EPLL_VAL
	str	r1, [r0, #EPLL_CON_OFFSET]
// 3、设置CLK_DIV1寄存器分频
	/*******lxg added***********************/
	ldr   	r1, [r0, #CLK_DIV1_OFFSET]
	ldr	r2, =CLK_DIV1_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV1_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV1_OFFSET]
// 3、设置CLK_DIV2寄存器分频
	ldr   	r1, [r0, #CLK_DIV2_OFFSET]
	ldr	r2, =CLK_DIV2_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV2_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV2_OFFSET]
// 3、设置CLK_DIV4寄存器分频
	ldr   	r1, [r0, #CLK_DIV4_OFFSET]
	ldr	r2, =CLK_DIV4_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV4_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV4_OFFSET]
// 3、设置CLK_DIV6寄存器分频
	ldr   	r1, [r0, #CLK_DIV6_OFFSET]
	ldr	r2, =CLK_DIV6_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV6_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV6_OFFSET]
	/*******end*****************/
	/*******end*****************/
#if defined(CONFIG_EVT1)
	ldr	r1, =AFC_ON
	str	r1, [r0, #APLL_CON1_OFFSET]
#endif
	mov	r1, #0x10000
1:	subs	r1, r1, #1
	bne	1b

#if defined(CONFIG_CHECK_MPLL_LOCK)
	/* MPLL software workaround */
	ldr	r1, [r0, #MPLL_CON_OFFSET]
	orr     r1, r1, #(1<<28)
	str	r1, [r0, #MPLL_CON_OFFSET]

	mov	r1, #0x100
1:	subs	r1, r1, #1
	bne	1b

	ldr	r1, [r0, #MPLL_CON_OFFSET]
	and	r1, r1, #(1<<29)
	cmp	r1, #(1<<29)
	bne 	retryloop

	/* H/W lock detect disable */
	ldr	r1, [r0, #MPLL_CON_OFFSET]
	bic     r1, r1, #(1<<28)
	str	r1, [r0, #MPLL_CON_OFFSET]
#endif
///5、319-329行 设置各种时钟开关，使用PLL
	ldr	r1, [r0, #CLK_SRC0_OFFSET]
	//ldr	r2, =0x10001111 //lxg changed.
   	ldr	r2, =0x00000111
	orr	r1, r1, r2
	str	r1, [r0, #CLK_SRC0_OFFSET]		// 将0x00000111值写入到CLK_SRC0寄存器中

	// added by terry 2012.12.4 for camera 
	ldr r1, [r0, #CLK_SRC1_OFFSET]
	bic r1, r1, #(0xf<<12)
	orr r1, r1, #(0x1<<12) // 1左移12位，0001: XusbXTI 得到原始时钟是从XusbXTI开始的
	str r1, [r0, #CLK_SRC1_OFFSET]

#if defined(CONFIG_MCP_AC)

	/* CLK_SRC6[25:24] -> OneDRAM clock sel = MPLL */
	ldr	r1, [r0, #CLK_SRC6_OFFSET]
	bic	r1, r1, #(0x3<<24)
	orr	r1, r1, #0x01000000
	str	r1, [r0, #CLK_SRC6_OFFSET]

	/* CLK_DIV6[31:28] -> 4=1/5, 3=1/4(166MHZ@667MHz), 2=1/3 */
	ldr	r1, [r0, #CLK_DIV6_OFFSET]
	bic	r1, r1, #(0xF<<28)
	bic	r1, r1, #(0x7<<12)	@; ONENAND_RATIO: 0
	orr	r1, r1, #0x30000000
	str	r1, [r0, #CLK_DIV6_OFFSET]

#elif defined (CONFIG_MCP_H)

	/* CLK_SRC6[25:24] -> OneDRAM clock sel = 00:SCLKA2M, 01:SCLKMPLL */
	ldr	r1, [r0, #CLK_SRC6_OFFSET]
	bic	r1, r1, #(0x3<<24)
	orr	r1, r1, #0x00000000
	str	r1, [r0, #CLK_SRC6_OFFSET]

	/* CLK_DIV6[31:28] -> 4=1/5, 3=1/4(166MHZ@667MHz), 2=1/3 */
	ldr	r1, [r0, #CLK_DIV6_OFFSET]
	bic	r1, r1, #(0xF<<28)
	bic	r1, r1, #(0x7<<12)	@; ONENAND_RATIO: 0
	orr	r1, r1, #0x00000000
	str	r1, [r0, #CLK_DIV6_OFFSET]	

#elif defined (CONFIG_MCP_B) || defined (CONFIG_MCP_D)

	/* CLK_SRC6[25:24] -> OneDRAM clock sel = 00:SCLKA2M, 01:SCLKMPLL */
	ldr	r1, [r0, #CLK_SRC6_OFFSET]
	bic	r1, r1, #(0x3<<24)
	orr	r1, r1, #0x01000000
	str	r1, [r0, #CLK_SRC6_OFFSET]

	/* CLK_DIV6[31:28] -> 4=1/5, 3=1/4(166MHZ@667MHz), 2=1/3 */
	ldr	r1, [r0, #CLK_DIV6_OFFSET]
	bic	r1, r1, #(0xF<<28)
	bic	r1, r1, #(0x7<<12)	@; ONENAND_RATIO: 0
	orr	r1, r1, #0x30000000
	str	r1, [r0, #CLK_DIV6_OFFSET]

#elif defined (CONFIG_MCP_SINGLE)

	/* CLK_DIV6 */
	/*ldr	r1, [r0, #CLK_DIV6_OFFSET]
	bic	r1, r1, #(0x7<<12)	@; ONENAND_RATIO: 0
	str	r1, [r0, #CLK_DIV6_OFFSET]*/ //lxg mask

#endif	
	// 做函数返回
	mov	pc, lr


/*
 * uart_asm_init: Initialize UART in asm mode, 115200bps fixed.
 * void uart_asm_init(void)
 */
uart_asm_init:					// 串口初始化程序
	// 串口0初始化完了，就能打印出信息了
	/* set GPIO(GPA) to enable UART */
	@ GPIO setting for UART		// 初始化Tx Rx对应的GPIO引脚
	ldr	r0, =ELFIN_GPIO_BASE
	ldr	r1, =0x22222222
	str   	r1, [r0, #GPA0CON_OFFSET]

	ldr     r1, =0x2222
	str     r1, [r0, #GPA1CON_OFFSET]

	// HP V210 use. SMDK not use.
#if defined(CONFIG_VOGUES)
	ldr    r1, =0x100
	str    r1, [r0, #GPC0CON_OFFSET]

	ldr    r1, =0x4
	str    r1, [r0, #GPC0DAT_OFFSET]
#endif
// 几个关键寄存器的设置	UCON0	ULCON0	UMCON0	UFCON0	UBRDIV0	UDIVSLOT0
	ldr	r0, =ELFIN_UART_CONSOLE_BASE		@0xE2900000
	mov	r1, #0x0
	str	r1, [r0, #UFCON_OFFSET]
	str	r1, [r0, #UMCON_OFFSET]
// 这些设置值对应的含义，可以到数据手册中寄存器部分进行查看(section8.1.6)
	mov	r1, #0x3
	str	r1, [r0, #ULCON_OFFSET]

	ldr	r1, =0x3c5
	str	r1, [r0, #UCON_OFFSET]

	ldr	r1, =UART_UBRDIV_VAL		// 计算公式在数据手册的879页，值为34
	str	r1, [r0, #UBRDIV_OFFSET]

	ldr	r1, =UART_UDIVSLOT_VAL
	str	r1, [r0, #UDIVSLOT_OFFSET]

	ldr	r1, =0x4f4f4f4f
	// UTXH发送寄存器，为了做阶段性调试
	str	r1, [r0, #UTXH_OFFSET]		@'O'	// 初始化完了后通过串口发送一个串口'O'
	// 做函数返回
	mov	pc, lr

/*
 * Nand Interface Init for SMDKC110
 */
nand_asm_init:

	/* Setting GPIO for NAND */
	/* This setting is NAND initialze code at booting time in iROM. */

	ldr	r0, =ELFIN_GPIO_BASE
	
	ldr	r1, [r0, #MP01CON_OFFSET]
	bic	r1, r1, #(0xf<<8)
	orr	r1, r1, #(0x3<<8)
	str	r1, [r0, #MP01CON_OFFSET]

	ldr	r1, [r0, #MP01PUD_OFFSET]
	bic	r1, r1, #(0x3<<4)
	str	r1, [r0, #MP01PUD_OFFSET]

	ldr	r1, [r0, #MP03CON_OFFSET]
	bic	r1, r1, #0xFFFFFF
	ldr	r2, =0x22222222
	orr	r1, r1, r2
	str	r1, [r0, #MP03CON_OFFSET]

	ldr	r1, [r0, #MP03PUD_OFFSET]
	ldr	r2, =0x3fff
	bic	r1, r1, r2
	str	r1, [r0, #MP03PUD_OFFSET]

	ldr	r0, =ELFIN_NAND_BASE

	ldr	r1, [r0, #NFCONF_OFFSET]
	ldr	r2, =0x777F
	bic	r1, r1, r2
	ldr	r2, =NFCONF_VAL
	orr	r1, r1, r2
	str	r1, [r0, #NFCONF_OFFSET]

	ldr	r1, [r0, #NFCONT_OFFSET]
	ldr	r2, =0x707C7
	bic	r1, r1, r2
	ldr	r2, =NFCONT_VAL
	orr	r1, r1, r2
	str	r1, [r0, #NFCONT_OFFSET]

	ldr	r1, [r0, #NFCONF_OFFSET]
	orr	r1, r1, #0x70
	orr	r1, r1, #0x7700
	str     r1, [r0, #NFCONF_OFFSET]

	ldr	r1, [r0, #NFCONT_OFFSET]
	orr	r1, r1, #0x03
	str     r1, [r0, #NFCONT_OFFSET]

	mov	pc, lr

/*
 * Setting TZPC[TrustZone Protection Controller]
 */
tzpc_init:

	ldr	r0, =ELFIN_TZPC0_BASE
 	mov	r1, #0x0
 	str	r1, [r0]
 	mov	r1, #0xff
 	str	r1, [r0, #TZPC_DECPROT0SET_OFFSET]
 	str	r1, [r0, #TZPC_DECPROT1SET_OFFSET]
	str	r1, [r0, #TZPC_DECPROT2SET_OFFSET] 	

 	ldr 	r0, =ELFIN_TZPC1_BASE
 	str	r1, [r0, #TZPC_DECPROT0SET_OFFSET]
 	str	r1, [r0, #TZPC_DECPROT1SET_OFFSET]
	str	r1, [r0, #TZPC_DECPROT2SET_OFFSET] 	

 	ldr	r0, =ELFIN_TZPC2_BASE
 	str	r1, [r0, #TZPC_DECPROT0SET_OFFSET]
 	str	r1, [r0, #TZPC_DECPROT1SET_OFFSET]
	str	r1, [r0, #TZPC_DECPROT2SET_OFFSET]
	str	r1, [r0, #TZPC_DECPROT3SET_OFFSET] 

 	ldr	r0, =ELFIN_TZPC3_BASE
 	str	r1, [r0, #TZPC_DECPROT0SET_OFFSET]
 	str	r1, [r0, #TZPC_DECPROT1SET_OFFSET]
	str	r1, [r0, #TZPC_DECPROT2SET_OFFSET] 	

 	mov	pc, lr

/*
 * OneNAND Interface Init
 */
onenandcon_init:

	@; GPIO setting for OneNAND
	ldr	r0, =ELFIN_GPIO_BASE	@0xE0200000
	ldr	r1, [r0, #MP01CON_OFFSET]
	orr	r1, r1, #0x00550000
	str	r1, [r0, #MP01CON_OFFSET]

	ldr	r1, [r0, #MP03CON_OFFSET]
	orr	r1, r1, #0x0550
	orr	r1, r1, #0x00550000
	str	r1, [r0, #MP03CON_OFFSET]

	ldr	r1, =0xFFFF
	str	r1, [r0, #MP01DRV_SR_OFFSET]
	str	r1, [r0, #MP03DRV_SR_OFFSET]
	str	r1, [r0, #MP06DRV_SR_OFFSET]
	str	r1, [r0, #MP07DRV_SR_OFFSET]

wait_orwb:
	@; Read ONENAND_IF_STATUS
	ldr	r0, =ELFIN_ONENANDCON_BASE	@; 0xB0600000
	ldr	r1, [r0, #ONENAND_IF_STATUS_OFFSET]
	bic	r1, r1, #0xFFFFFFFE
	cmp	r1, #0x0

	@; ORWB != 0x0
	bne	wait_orwb

	@; write new configuration to onenand system configuration1 register
	ldr	r1, =0xF006			@; Sync.
	ldr	r2, =(ELFIN_ONENAND_BASE+0x1E442)	@; 0x1E442(REG_SYS_CONF1)
	strh	r1, [r2]

	@; read one dummy halfword
	ldrh	r1, [r2]
	ldrh	r1, [r2]

	@; write new configuration to ONENAND_IF_CTRL
	ldr	r0, =ELFIN_ONENANDCON_BASE	@; 0xB0600000
	@;ldr	r1, =0x2F006			@; ONENAND_IF_CTRL_REG_VAL (GCE off)
	ldr	r1, =0x402F006			@; ONENAND_IF_CTRL_REG_VAL (GCE on)
	str	r1, [r0, #ONENAND_IF_CTRL_OFFSET]

	mov	pc, lr


#ifdef CONFIG_ENABLE_MMU

	#ifdef CONFIG_MCP_SINGLE
/*
 * MMU Table for SMDKC110
 * 0x0000_0000 -- 0xBFFF_FFFF => Not Allowed
 * 0xB000_0000 -- 0xB7FF_FFFF => A:0xB000_0000 -- 0xB7FF_FFFF
 * 0xC000_0000 -- 0xC7FF_FFFF => A:0x3000_0000 -- 0x37FF_FFFF
 * 0xC800_0000 -- 0xDFFF_FFFF => Not Allowed
 * 0xE000_0000 -- 0xFFFF_FFFF => A:0xE000_0000 -- 0XFFFF_FFFF
 */

	/* form a first-level section entry */
	// FL_SECTION_ENTRY宏名；宏接收的5个参数base,ap,d,c,b
.macro FL_SECTION_ENTRY base,ap,d,c,b			// .macro是汇编中用来定义宏的
	.word (\base << 20) | (\ap << 10) | \
	      (\d << 5) | (1<<4) | (\c << 3) | (\b << 2) | (1<<1)		// \base << 20是M为单位对齐
.endm		// 终止宏
.section .mmudata, "a"
	.align 14
	// the following alignment creates the mmu table at address 0x4000.
	.globl mmu_table
mmu_table:
// 宏观上理解转换表: 整个转换表可以看作是一个int类型的数组，数组中的一个元素就是一个表索引和表项的单元。
// 数组中的元素值就是表项，这个元素的数组下标就是表索引。
// 下面就是在建立转换表，也就是建立数组。
// ARM的段式映射中长度为1MB，因此一个映射单元只能管1MB内存，那我们整个4G范围内需要4G/1MB=4096个映射单元
// 也就是说这个数组的元素个数是4096。实际上我们做的时候并没有依次单个处理这4096个单元，而是把4096个分成几部分，
// 然后每部分用for循环做相同的处理。

// 从转换表得出结论: 虚拟地址映射只是把虚拟地址的c0000000开头的256MB映射到了DMC0的30000000开头的
// 256MB物理内存上去了。其他的虚拟地址空间根本没动，还是原样子。
	.set __base,0		// 把__base设置为0
	// Access for iRAM
	// .rept是伪指令，用来重复；相当于for循环；
	.rept 0x100							// 循环开始	建立了0x100个单元，建立了256个映射，也就是256MB
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr								// 循环结束

	// Not Allowed
	.rept 0x200 - 0x100
	// 说明这段的虚拟地址是不可访问的，映射到物理地址为0
	.word 0x00000000
	.endr

	.set __base,0x200
	// should be accessed
	.rept 0x600 - 0x200
	FL_SECTION_ENTRY __base,3,0,1,1
	.set __base,__base+1
	.endr

	.rept 0x800 - 0x600
	.word 0x00000000
	.endr

	.set __base,0x800
	// should be accessed
	.rept 0xb00 - 0x800
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr

/*	.rept 0xc00 - 0xb00
	.word 0x00000000
	.endr */

	.set __base,0xB00
	.rept 0xc00 - 0xb00
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr

	.set __base,0x300
	//.set __base,0x200
	// 256MB for SDRAM with cacheable
	.rept 0xD00 - 0xC00
	// 这256MB，是将0xc00 - 0xd00映射到0x3000 - 0x4000物理地址处
	FL_SECTION_ENTRY __base,3,0,1,1
	.set __base,__base+1
	.endr

	// access is not allowed.
	@.rept 0xD00 - 0xC80
	@.word 0x00000000
	@.endr

	.set __base,0xD00
	// 1:1 mapping for debugging with non-cacheable
	// 0x1000就是4G
	.rept 0x1000 - 0xD00
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr	
	
	#else	// CONFIG_MCP_AC, CONFIG_MCP_H, CONFIG_MCP_B

/*
 * MMU Table for SMDKC110
 * 0x0000_0000 -- 0xBFFF_FFFF => Not Allowed
 * 0xB000_0000 -- 0xB7FF_FFFF => A:0xB000_0000 -- 0xB7FF_FFFF
 * 0xC000_0000 -- 0xC7FF_FFFF => A:0x3000_0000 -- 0x37FF_FFFF
 * 0xC800_0000 -- 0xDFFF_FFFF => Not Allowed
 * 0xE000_0000 -- 0xFFFF_FFFF => A:0xE000_0000 -- 0XFFFF_FFFF
 */

	/* form a first-level section entry */
.macro FL_SECTION_ENTRY base,ap,d,c,b
	.word (\base << 20) | (\ap << 10) | \
	      (\d << 5) | (1<<4) | (\c << 3) | (\b << 2) | (1<<1)
.endm
.section .mmudata, "a"
	.align 14
	// the following alignment creates the mmu table at address 0x4000.
	.globl mmu_table
mmu_table:
	.set __base,0
	// Access for iRAM
	.rept 0x100
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr

	// Not Allowed
	.rept 0x300 - 0x100
	.word 0x00000000
	.endr

	.set __base,0x300
	// should be accessed
	.rept 0x400 - 0x300
	//.rept 0x350 - 0x300
	FL_SECTION_ENTRY __base,3,0,1,1
	.set __base,__base+1
	.endr

	// Not Allowed
	//.rept 0x400 - 0x350
	//.word 0x00000000
	//.endr

	// DRAM - DMC1 area - used for STL_write : djpark (20090729)
	.set __base,0x400
	// should be accessed
	.rept 0x500 - 0x400
	FL_SECTION_ENTRY __base,3,0,1,1
	.set __base,__base+1
	.endr

	.rept 0x800 - 0x500
	.word 0x00000000
	.endr

	.set __base,0x800
	// should be accessed
	.rept 0xb00 - 0x800
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr

	.set __base,0xB00
	.rept 0xc00 - 0xb00
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr

	.set __base,0x300
	// 80MB for SDRAM with cacheable
	.rept 0xd00 - 0xC00
	//.rept 0xC50 - 0xC00
	FL_SECTION_ENTRY __base,3,0,1,1
	.set __base,__base+1
	.endr

	// Not Allowed
	@.rept 0xD00 - 0xC80
	@.word 0x00000000
	@.endr
	
	// Not Allowed
	//.rept 0xD00 - 0xC50
	//.word 0x00000000
	//.endr

	.set __base,0xD00
	// 1:1 mapping for debugging with non-cacheable
	.rept 0x1000 - 0xD00
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr
	#endif
#endif

