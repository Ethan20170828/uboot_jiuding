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
// lowlevel_init�������ǵײ�Ĵ���
	.globl lowlevel_init
lowlevel_init:
	// ��lr����ѹջ����Ϊ������ʱ��lr�ﱣ����Ǻ����ķ��ص�ַ��
	// ������������滹�������������������Ҫ��lowlevel_init�����ĵ�ַ����ѹջ
	// ����ִ����lowlevel_init�����󣬿��Է��ػ�ȥ
	// �������lr����ѹջ�����������������ʱ��lowlevel_init��������lr�еĵ�ַ�ᱻ����
	// Ҳ���ǻᶪ
	push	{lr}	// push�ǽ�ջ

	/* check reset status ��鸴λ״̬ ���������������޹�*/
	// ����CPU�������ָ�λ�����Ʃ��ֱ�����ϵ硢��������˯��(�͹���)״̬�µĻ��ѵ�
	// ��Щ��������ڸ�λ����������Ҫ�ڸ�λ�����м�⸴λ״̬���жϵ������������
	// �ж����ָ�λ����������: ���ϵ�ʱDDR����Ҫ��ʼ�������õ�
	// �����������ߵ͹���״̬�µĸ�λ����Ҫ�ٴγ�ʼ��DDR��
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE+RST_STAT_OFFSET)	// �ѼĴ����ĵ�ַд�뵽r0��
	ldr	r1, [r0]
	bic	r1, r1, #0xfff6ffff
	cmp	r1, #0x10000
	beq	wakeup_reset_pre
	cmp	r1, #0x80000
	beq	wakeup_reset_from_didle

	/* IO Retention release IO�ָ������������������޹�*/
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE + OTHERS_OFFSET)	// �ѼĴ����ĵ�ַд�뵽r0��
	ldr	r1, [r0]
	ldr	r2, =IO_RET_REL
	orr	r1, r1, r2
	str	r1, [r0]

	/* Disable Watchdog ��WTCON��bit5д��0����*/
	ldr	r0, =ELFIN_WATCHDOG_BASE	/* 0xE2700000 	�ѼĴ����ĵ�ַд�뵽r0��*/
	mov	r1, #0		// ��r1��д0
	str	r1, [r0]	// ��r1�е�ֵд��r0��ָ��ļĴ�����

	/* SRAM(2MB) init for SMDKC110 	�������ⲿ�����SRAM������Ҫ���ⲿSRAM��ʼ��	���������������޹�*/
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

	/* GPJ4 SROM_ADDR_16to21 	�������ⲿ�����SROM������Ҫ���ⲿSROM��ʼ��	���������������޹�*/
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

	/* PS_HOLD pin(GPH0_0) set to high��PS_HOLD�ǿ����幩������   */
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE + PS_HOLD_CONTROL_OFFSET)
	ldr	r1, [r0]		// ��r0ָ��ļĴ����е���д��r1��
	orr	r1, r1, #0x300	// r1��0x300ȡ��
	orr	r1, r1, #0x1	// r1��1ȡ��	
	str	r1, [r0]		// ��r1�õ���ֵд��r0��ָ��ļĴ�����

// ����֮ǰ��lowlevel_init�����в�û����̫�������������(���˹ؿ��Ź��������幩��������)
// �����Ժ�lowlevel_init�����ſ�ʼ����������Ĳ���
	/* when we already run in ram, we don't need to relocate U-Boot.
	 * and actually, memory controller must be configured before U-Boot
	 * is running in ram.
	 */		
	// 130-141�⼸�д����жϵ�ǰ����ִ�е�λ������iSAM������DDR��
	// ΪʲôҪ������ж�? 
	// ԭ��1: BL1(uboot��ǰһ����)��SRAM����һ�ݣ���DDR��Ҳ��һ�ݣ�
	// ��������������ô��ǰ����Ӧ������SRAM�����е�BL1��
	// ����ǵ͹���״̬�ĸ�λ��ʱ��Ӧ�þ�����DDR�����еġ�
	// ԭ��2: �����ж���ǰ���д���ĵ�ַ���õģ�����ָ��������������
	// Ʃ����lowlevel_init.S���ж���ǰ��������е�ַ������Ϊ��ȷ��Ҫ��Ҫ
	// ִ��ʱ�ӳ�ʼ���ͳ�ʼ��DDR�Ĵ��롣����ǵ�ǰ��������SRAM�У�˵��
	// ����������ôʱ�Ӻ�DDR����Ҫ��ʼ���������ǰ��������DDR�У���ô˵��
	// ������������ô˵��ʱ�Ӻ�DDR���ǲ����ٴγ�ʼ����
	ldr	r0, =0xff000fff
	// ����: ��pc��ֵ�е�ĳЩbitλ��0��ʣ��һЩ�����bitλ��ֵ��r1���൱��r1 = pc & ~(0xff000fff)
	bic	r1, pc, r0		/* r0 <- current base addr of code ��pc�ǵ�ǰ����ĵ�ַ*/
	// �����ӵ�ַ���ص�r2�У����ӵ�ַ(�����ַ)��Ҳ����DDR�еĵ�ַ����Ҳ��˵����������DDR�Ѿ���ʼ������
	ldr	r2, _TEXT_BASE		/* r1 <- original base addr in ram ��*/
	// ��r2����Ӧλ��0ʣ���ض�λ
	bic	r2, r2, r0		/* r0 <- current base addr of code */
	// �Աȵķ�����pc���Լ��߹�����Щ��ַȥ����Ȼ�������ӵ�ַ���жԱȣ�������е���Щ��ͬ
	cmp     r1, r2                  /* compare r0, r1                  */
	// ���r1��r2��ȣ���ǰ������DDR�У�������������Ҫ��ʼ��ʱ�Ӻ�DDR
	// ���r1��r2����ȣ���ǰ������SRAM�У�����������Ҫ��ʼ��ʱ�Ӻ�DDR
	beq     1f			/* r0 == r1 then skip sdram init   */

	/* init system clock */
	bl system_clock_init	// ��ʼ��ʱ��

	/* Memory initialize */
	bl mem_ctrl_asm_init	// ��ʼ��DDR
	
1:
	/* for UART */
	bl uart_asm_init
	// trust zone��ʼ��
	bl tzpc_init			// ����Ҫ���������ܼ���

#if defined(CONFIG_ONENAND)
	bl onenandcon_init
#endif

#if defined(CONFIG_NAND)
	/* simple init for NAND */
	bl nand_asm_init
#endif

	/* check reset status  ��鸴λ״̬*/
	
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE+RST_STAT_OFFSET)
	ldr	r1, [r0]
	bic	r1, r1, #0xfffeffff
	cmp	r1, #0x10000
	beq	wakeup_reset_pre

	/* ABB disable */
	ldr	r0, =0xE010C300
	orr	r1, r1, #(0x1<<23)
	str	r1, [r0]

	/* Print 'K' ��ӡK��Ҳ��Ϊ�������ڵ��ԣ�*/
	// lowlevel_initִ�������û����ô�ͻᴮ�ڴ�ӡ"OK"�����������uboot�п���������������Ϣ
	ldr	r0, =ELFIN_UART_CONSOLE_BASE
	ldr	r1, =0x4b4b4b4b
	str	r1, [r0, #UTXH_OFFSET]		// ����ǰͨ�����ڴ�ӡ'K'
	// pop�ǳ�ջ
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
	// ����Ĺ��̾��ǳ�ʼ��ʱ��
	ldr	r0, =ELFIN_CLOCK_POWER_BASE	@0xe0100000
// 1������������Ҫ����ʱ������APLL(Control MUXAPLL)
	/* Set Mux to FIN */
	ldr	r1, =0x0
	str	r1, [r0, #CLK_SRC0_OFFSET]		// ����CLK_SRC0�Ĵ���������ȫ��д0
// 2������APLL����ʱ�䣬����PLL��ʱ�Ӵ�Fin������Ŀ��Ƶ��ʱ����Ҫһ��ʱ�䣬������ʱ��
	ldr	r1,	=APLL_LOCKTIME_VAL
	str	r1,	[r0, #APLL_LOCK_OFFSET]

	/********lxg added*********************/
	ldr	r0, =ELFIN_CLOCK_POWER_BASE	@0xe0100000
// 2������MPLL����ʱ��
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
// 258-259�н�APLL_CON0�Ĵ�����MPLL_CON�Ĵ�������λ����λ0
	ldr	r1, =0x0
	str	r1, [r0, #MPLL_CON_OFFSET]
// 3��261-263�У�����CLK_DIV0�Ĵ�����Ƶ
	ldr   	r1, [r0, #CLK_DIV0_OFFSET]
	ldr	r2, =CLK_DIV0_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV0_VAL	// ��CLK_DIV0_VAL��ֵд��CLK_DIV0�Ĵ����У��Ӷ�����PCLK_PSYS/HCLK_PSYS/PCLK_DSYS/HCLK_DSYS/PCLK_MSYS/HCLK_MSYS/SCLKA2M/ARMCLK��ֵ
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV0_OFFSET]
///4��269-280������APLL/MPLL/VPLL/EPLL����Ҫ����PLL�ı�Ƶϵ���������������24MHz��ԭʼƵ�ʿ��Եõ��������Ƶ��
	ldr	r1, =APLL_VAL		// ���÷����ο������ֲ�ʱ�Ӳ���(372)
	str	r1, [r0, #APLL_CON0_OFFSET]

	ldr	r1, =MPLL_VAL
	str	r1, [r0, #MPLL_CON_OFFSET]

	ldr	r1, =VPLL_VAL
	str	r1, [r0, #VPLL_CON_OFFSET]

	/*******lxg added***********************/
	ldr	r1, =EPLL_VAL
	str	r1, [r0, #EPLL_CON_OFFSET]
// 3������CLK_DIV1�Ĵ�����Ƶ
	/*******lxg added***********************/
	ldr   	r1, [r0, #CLK_DIV1_OFFSET]
	ldr	r2, =CLK_DIV1_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV1_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV1_OFFSET]
// 3������CLK_DIV2�Ĵ�����Ƶ
	ldr   	r1, [r0, #CLK_DIV2_OFFSET]
	ldr	r2, =CLK_DIV2_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV2_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV2_OFFSET]
// 3������CLK_DIV4�Ĵ�����Ƶ
	ldr   	r1, [r0, #CLK_DIV4_OFFSET]
	ldr	r2, =CLK_DIV4_MASK
	bic	r1, r1, r2

	ldr	r2, =CLK_DIV4_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV4_OFFSET]
// 3������CLK_DIV6�Ĵ�����Ƶ
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
///5��319-329�� ���ø���ʱ�ӿ��أ�ʹ��PLL
	ldr	r1, [r0, #CLK_SRC0_OFFSET]
	//ldr	r2, =0x10001111 //lxg changed.
   	ldr	r2, =0x00000111
	orr	r1, r1, r2
	str	r1, [r0, #CLK_SRC0_OFFSET]		// ��0x00000111ֵд�뵽CLK_SRC0�Ĵ�����

	// added by terry 2012.12.4 for camera 
	ldr r1, [r0, #CLK_SRC1_OFFSET]
	bic r1, r1, #(0xf<<12)
	orr r1, r1, #(0x1<<12) // 1����12λ��0001: XusbXTI �õ�ԭʼʱ���Ǵ�XusbXTI��ʼ��
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
	// ����������
	mov	pc, lr


/*
 * uart_asm_init: Initialize UART in asm mode, 115200bps fixed.
 * void uart_asm_init(void)
 */
uart_asm_init:					// ���ڳ�ʼ������
	// ����0��ʼ�����ˣ����ܴ�ӡ����Ϣ��
	/* set GPIO(GPA) to enable UART */
	@ GPIO setting for UART		// ��ʼ��Tx Rx��Ӧ��GPIO����
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
// �����ؼ��Ĵ���������	UCON0	ULCON0	UMCON0	UFCON0	UBRDIV0	UDIVSLOT0
	ldr	r0, =ELFIN_UART_CONSOLE_BASE		@0xE2900000
	mov	r1, #0x0
	str	r1, [r0, #UFCON_OFFSET]
	str	r1, [r0, #UMCON_OFFSET]
// ��Щ����ֵ��Ӧ�ĺ��壬���Ե������ֲ��мĴ������ֽ��в鿴(section8.1.6)
	mov	r1, #0x3
	str	r1, [r0, #ULCON_OFFSET]

	ldr	r1, =0x3c5
	str	r1, [r0, #UCON_OFFSET]

	ldr	r1, =UART_UBRDIV_VAL		// ���㹫ʽ�������ֲ��879ҳ��ֵΪ34
	str	r1, [r0, #UBRDIV_OFFSET]

	ldr	r1, =UART_UDIVSLOT_VAL
	str	r1, [r0, #UDIVSLOT_OFFSET]

	ldr	r1, =0x4f4f4f4f
	// UTXH���ͼĴ�����Ϊ�����׶��Ե���
	str	r1, [r0, #UTXH_OFFSET]		@'O'	// ��ʼ�����˺�ͨ�����ڷ���һ������'O'
	// ����������
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
	// FL_SECTION_ENTRY����������յ�5������base,ap,d,c,b
.macro FL_SECTION_ENTRY base,ap,d,c,b			// .macro�ǻ��������������
	.word (\base << 20) | (\ap << 10) | \
	      (\d << 5) | (1<<4) | (\c << 3) | (\b << 2) | (1<<1)		// \base << 20��MΪ��λ����
.endm		// ��ֹ��
.section .mmudata, "a"
	.align 14
	// the following alignment creates the mmu table at address 0x4000.
	.globl mmu_table
mmu_table:
// ���������ת����: ����ת�������Կ�����һ��int���͵����飬�����е�һ��Ԫ�ؾ���һ���������ͱ���ĵ�Ԫ��
// �����е�Ԫ��ֵ���Ǳ�����Ԫ�ص������±���Ǳ�������
// ��������ڽ���ת������Ҳ���ǽ������顣
// ARM�Ķ�ʽӳ���г���Ϊ1MB�����һ��ӳ�䵥Ԫֻ�ܹ�1MB�ڴ棬����������4G��Χ����Ҫ4G/1MB=4096��ӳ�䵥Ԫ
// Ҳ����˵��������Ԫ�ظ�����4096��ʵ������������ʱ��û�����ε���������4096����Ԫ�����ǰ�4096���ֳɼ����֣�
// Ȼ��ÿ������forѭ������ͬ�Ĵ�����

// ��ת�����ó�����: �����ַӳ��ֻ�ǰ������ַ��c0000000��ͷ��256MBӳ�䵽��DMC0��30000000��ͷ��
// 256MB�����ڴ���ȥ�ˡ������������ַ�ռ����û��������ԭ���ӡ�
	.set __base,0		// ��__base����Ϊ0
	// Access for iRAM
	// .rept��αָ������ظ����൱��forѭ����
	.rept 0x100							// ѭ����ʼ	������0x100����Ԫ��������256��ӳ�䣬Ҳ����256MB
	FL_SECTION_ENTRY __base,3,0,0,0
	.set __base,__base+1
	.endr								// ѭ������

	// Not Allowed
	.rept 0x200 - 0x100
	// ˵����ε������ַ�ǲ��ɷ��ʵģ�ӳ�䵽������ַΪ0
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
	// ��256MB���ǽ�0xc00 - 0xd00ӳ�䵽0x3000 - 0x4000������ַ��
	FL_SECTION_ENTRY __base,3,0,1,1
	.set __base,__base+1
	.endr

	// access is not allowed.
	@.rept 0xD00 - 0xC80
	@.word 0x00000000
	@.endr

	.set __base,0xD00
	// 1:1 mapping for debugging with non-cacheable
	// 0x1000����4G
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
