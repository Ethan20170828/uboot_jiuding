/*
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

#ifndef	__ASM_GBL_DATA_H
#define __ASM_GBL_DATA_H
/*
 * The following data structure is placed in some memory wich is
 * available very early after boot (like DPRAM on MPC8xx/MPC82xx, or
 * some locked parts of the data cache) to allow for a minimum set of
 * global variables during system initialization (until we have set
 * up the memory controller so that we can use RAM).
 *
 * Keep it *SMALL* and remember to set CFG_GBL_DATA_SIZE > sizeof(gd_t)
 */
// 把所有用到的全局变量放在了一起
typedef	struct	global_data {
	// bd_t开发板板级相关信息
	bd_t		*bd;
	unsigned long	flags;		// 标志位
	unsigned long	baudrate;	// 波特率
	unsigned long	have_console;	/* serial_init() was called 值为0或者1，当前的控制台是否打开*/
	unsigned long	reloc_off;	/* Relocation Offset 重定位的偏移量*/
	unsigned long	env_addr;	/* Address  of Environment struct 环境变量结构体偏移量*/
	unsigned long	env_valid;	/* Checksum of Environment valid? 检查内存中的环境变量是否可以使用*/
	unsigned long	fb_base;	/* base address of frame buffer 缓存的基地址*/
#ifdef CONFIG_VFD
	unsigned char	vfd_type;	/* display type */
#endif
#if 0
	unsigned long	cpu_clk;	/* CPU clock in Hz!		*/
	unsigned long	bus_clk;
	phys_size_t	ram_size;	/* RAM size */
	unsigned long	reset_status;	/* reset status register at boot */
#endif
	void		**jt;		/* jump table 跳转表，uboot中基本不用*/
} gd_t;

/*
 * Global Data Flags
 */
#define	GD_FLG_RELOC	0x00001		/* Code was relocated to RAM		*/
#define	GD_FLG_DEVINIT	0x00002		/* Devices have been initialized	*/
#define	GD_FLG_SILENT	0x00004		/* Silent mode				*/
#define	GD_FLG_POSTFAIL	0x00008		/* Critical POST test failed		*/
#define	GD_FLG_POSTSTOP	0x00010		/* POST seqeunce aborted		*/
#define	GD_FLG_LOGINIT	0x00020		/* Log Buffer has been initialized	*/
// 定义了一个全局变量名字叫gd，这个全局变量是一个指针类型，占4字节。
// 用volatile修饰表示可变的，用register修饰表示这个变量要尽量放到寄存器中，
// asm ("r8")是gcc支持的一种语法，意思就是要把gd放到寄存器r8中。
// 综合分析，DECLARE_GLOBAL_DATA_PTR就是定义了一个要放在寄存器r8中的全局变量，名字叫gd，类型是指向gd_t类型变量的指针。
// 为什么要定义为register? 因为这个全局变量gd(global data的简称)是uboot中很重要的一个全局变量(
// 准确的说这个全局变量是一个结构体，里面很多内容，这些内容加起来构成的结构体就是uboot中常用的所有的全局变量)
// 这个gd在程序中经常被访问，因此放在register中提升效率。因此纯粹是运行效率方面的考虑，和功能要求无关，并不是必须的
#define DECLARE_GLOBAL_DATA_PTR     register volatile gd_t *gd asm ("r8")

#endif /* __ASM_GBL_DATA_H */
