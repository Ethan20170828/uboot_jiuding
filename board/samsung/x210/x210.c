/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
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

#include <common.h>
#include <regs.h>
#include <asm/io.h>
#include <fb/mpadfb.h>//lxg added.

/* ------------------------------------------------------------------------- */
#define SMC9115_Tacs	(0x0)	// 0clk		address set-up
#define SMC9115_Tcos	(0x4)	// 4clk		chip selection set-up
#define SMC9115_Tacc	(0xe)	// 14clk	access cycle
#define SMC9115_Tcoh	(0x1)	// 1clk		chip selection hold
#define SMC9115_Tah	(0x4)	// 4clk		address holding time
#define SMC9115_Tacp	(0x6)	// 6clk		page mode access cycle
#define SMC9115_PMC	(0x0)	// normal(1data)page mode configuration

#define SROM_DATA16_WIDTH(x)	(1<<((x*4)+0))
#define SROM_WAIT_ENABLE(x)	(1<<((x*4)+1))
#define SROM_BYTE_ENABLE(x)	(1<<((x*4)+2))

/* ------------------------------------------------------------------------- */
#define DM9000_Tacs	(0x0)	// 0clk		address set-up
#define DM9000_Tcos	(0x4)	// 4clk		chip selection set-up
#define DM9000_Tacc	(0xE)	// 14clk	access cycle
#define DM9000_Tcoh	(0x1)	// 1clk		chip selection hold
#define DM9000_Tah	(0x4)	// 4clk		address holding time
#define DM9000_Tacp	(0x6)	// 6clk		page mode access cycle
#define DM9000_PMC	(0x0)	// normal(1data)page mode configuration


static inline void delay(unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n" "bne 1b":"=r" (loops):"0"(loops));
}

/*
 * Miscellaneous platform dependent initialisations
 */
// 因为网卡的驱动都是现成的正确的，移植的时候驱动是不需要改动的，
// 关键是这里的基本初始化。因为这些基本初始化是硬件相关的。
static void dm9000_pre_init(void)		// 函数主要是网卡的GPIO和端口的配置，而不是驱动
{
	unsigned int tmp;

#if defined(DM9000_16BIT_DATA)
	SROM_BW_REG &= ~(0xf << 4);
	SROM_BW_REG |= (1<<7) | (1<<6) | (1<<5) | (1<<4);
#else
	SROM_BW_REG &= ~(0xf << 4);
	SROM_BW_REG |= (0<<6) | (0<<5) | (0<<4);
#endif
	SROM_BC1_REG = ((0<<28)|(1<<24)|(5<<16)|(1<<12)|(4<<8)|(6<<4)|(0<<0));//uboot
	//SROM_BC1_REG = ((0<<28)|(0<<24)|(5<<16)|(0<<12)|(0<<8)|(0<<4)|(0<<0));//kernel
	tmp = MP01CON_REG;
	tmp &=~(0xf<<4);
	tmp |=(2<<4);
	MP01CON_REG = tmp;
}

// X210开发板相关的初始化
int board_init(void)
{
// 在这里声明是为了后面使用gd方便。
// 可以看出把gd声明定义一个宏的原因就是我们要到处使用gd，因此就要到处声明，定义成宏比较方便。
	DECLARE_GLOBAL_DATA_PTR;
#ifdef CONFIG_DRIVER_SMC911X
	smc9115_pre_init();
#endif

#ifdef CONFIG_DRIVER_DM9000		// 这个宏用来配置开发板的网卡的
	// 开发板移植uboot时，如果要移植网卡，主要的工作在这里
	// 因为驱动都是写好的，关键的就是这里的初始化函数，因为与每个网卡的硬件(GPIO和端口的配置)有关
	dm9000_pre_init();			// DM9000网卡初始化函数
#endif
	// bi_arch_number是board_info中的一个元素，含义是: 开发板的机器码。所谓机器码就是uboot给这个
	// 开发板定义的唯一编号。机器码的主要作用就是在uboot和Linux内核之间进行比对和适配。
	// 嵌入式设备中每一个设备的硬件都是定制化的，不能通用。
	// 嵌入式设备的高度定制化导致硬件和软件不能随便适配使用。
	// 这就告诉我们这个开发板移植的内核镜像绝对不能下载到另一个开发板去，否则也不能启动，就算
	// 启动也不能正常工作，有很多隐患。因此Linux做了个设置: 给每个开发板做了个唯一编号(机器码)，
	// 然后在uboot、Linux内核中都有一个软件维护的机器码编号。然后开发板、uboot、Linux三者去对比
	// 机器码，如果机器码对上了就启动，否则就不启动(因为软件认为我和这个硬件不适配)。
	// MACH_TYPE在x210_sd.h中定义，值是2456，并没有特殊含义，只是当前开发板对应的编号。这个编号
	// 就代表了X210这个开发板的机器码，将来这个开发板上面移植的Linux内核中的机器码也必须是2456，
	// 否则就启动不起来。
	// uboot中配置的这个机器码。会作为uboot给Linux内核的传参的一部分传给Linux内核，内核启动过程
	// 中会对比这个接收到的机器码，和自己本身的机器码相对比，如果相等kernel启动，如果不等则不启动。
	// 理论上来说，一个开发板的机器码不能自己随便定。理论来说有权利去发放这个机器码的只有uboot官方，
	// 所以我们做好一个开发板并且移植了uboot之后，理论上应该提交给uboot官方审核并发放机器码。但是国内
	// 的开发板基本都没有申请(主要原因是因为国内开发者英文不行，和国外开源社区接触比较少)，都是自己
	// 随便编号的。随便编号的问题就是有可能和别人的编号冲突，但是只要保证uboot和kernel中的编号是一致
	// 的，就不影响自己的开发板启动。
	gd->bd->bi_arch_number = MACH_TYPE;
	// bi_boot_params是board_info另一个主要元素，表示uboot给Linux kernel启动时的传参的内存地址。
	// 也就是说uboot给kernel传参的时候是这么传的: uboot事先将准备好的传参(字符串，就是bootargs)
	// 放在内存的一个地址处(就是bi_boot_params)，然后uboot就启动了内核(uboot在启动内核时真正是
	// 通过寄存器r0 r1 r2来直接传递参数的，其中有一个寄存器中就是bi_boot_params)。内核启动后从
	// 寄存器中读取bi_boot_params就知道了uboot给我传递的参数到底在内存的哪里。然后自己去内存的
	// 那个地方去找bootargs。
	// 说白了，就是bootargs放到bi_boot_params所在的内存中，这个内存的起始地址放到r2寄存器中
	// 内核通过读取r2寄存器，bi_boot_params所在内存的起始地址，得到通过指针解引用的方法来间接得到bootargs。
	// 经过计算得知，bi_boot_params的值为0x30000100，这个内存地址就被分配用来做内存传参了。
	// 所以在uboot的其他地方使用内存时要注意，千万不敢把这里给淹没了。
	gd->bd->bi_boot_params = (PHYS_SDRAM_1+0x100);			// 30000100

	return 0;
}

int dram_init(void)
{
	// DDR初始化
	// 注意: 这里的初始化DDR和汇编阶段lowlevel_init中初始化DDR是不同的
	// 当时是硬件的初始化，目的是让DDR可以开始工作。
	// 现在是软件结构中一些DDR相关的属性配置、地址设置的初始化，是纯软件层面的。
	// 软件层次初始化DDR的原因: 对于uboot来说，它怎么知道开发板上到底有几片DDR内存，每一片
	// 的起始地址、长度这些信息呢? 在uboot的设计中采用了一种简单直接有效的方式: 程序员在移植
	// uboot到一个开发板时，程序员自己在x210_sd.h中使用宏定义去配置出来板子上DDR内存的信息，
	// 然后uboot只要读取这些信息即可。(实际上还有另外一条思路: 就是uboot通过代码读取硬件信息
	// 来知道DDR配置，但是uboot没有这样。实际上PC的BIOS采用的是这种)
	// dram_init都是在给gd->bd里面关于DDR配置部分的全局变量赋值，让gd->bd数据记录下当前开发板
	// 的DDR的配置信息，以便uboot中使用内存。
	// 从代码来看，其实就是初始化gd->bd->bi_dram这个结构体数组。
	DECLARE_GLOBAL_DATA_PTR;

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;		// 第一片内存起始地址
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;	// 第一片内存的大小

#if defined(PHYS_SDRAM_2)
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;		// 第二片内存的起始地址
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;	// 第二片内存的大小
#endif

#if defined(PHYS_SDRAM_3)
	gd->bd->bi_dram[2].start = PHYS_SDRAM_3;
	gd->bd->bi_dram[2].size = PHYS_SDRAM_3_SIZE;
#endif

	return 0;
}

#ifdef BOARD_LATE_INIT
#if defined(CONFIG_BOOT_NAND)
int board_late_init (void)
{
	uint *magic = (uint*)(PHYS_SDRAM_1);
	char boot_cmd[100];

	if ((0x24564236 == magic[0]) && (0x20764316 == magic[1])) {
		sprintf(boot_cmd, "nand erase 0 40000;nand write %08x 0 40000", PHYS_SDRAM_1 + 0x8000);
		magic[0] = 0;
		magic[1] = 0;
		printf("\nready for self-burning U-Boot image\n\n");
		setenv("bootdelay", "0");
		setenv("bootcmd", boot_cmd);
	}

	return 0;
}
#elif defined(CONFIG_BOOT_MOVINAND)
int board_late_init (void)
{
	uint *magic = (uint*)(PHYS_SDRAM_1);
	char boot_cmd[100];
	int hc;

	hc = (magic[2] & 0x1) ? 1 : 0;

	if ((0x24564236 == magic[0]) && (0x20764316 == magic[1])) {
		sprintf(boot_cmd, "movi init %d %d;movi write u-boot %08x", magic[3], hc, PHYS_SDRAM_1 + 0x8000);
		magic[0] = 0;
		magic[1] = 0;
		printf("\nready for self-burning U-Boot image\n\n");
		setenv("bootdelay", "0");
		setenv("bootcmd", boot_cmd);
	}

	return 0;
}
#else
int board_late_init (void)
{
	return 0;
}
#endif
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
// 检查、确认开发板的意思，这个函数的作用就是检查当前开发板是哪个开发板并且打印出开发板的名字
int checkboard(void)
{
#ifdef CONFIG_MCP_SINGLE
#if defined(CONFIG_VOGUES)
	printf("\nBoard:   VOGUESV210\n");
#else
	printf("\nBoard:   X210\n");
#endif //CONFIG_VOGUES
#else
	printf("\nBoard:   X210\n");
#endif
	return (0);
}
#endif

#ifdef CONFIG_ENABLE_MMU

#ifdef CONFIG_MCP_SINGLE
ulong virt_to_phy_smdkc110(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xd0000000))
		return (addr - 0xc0000000 + MEMORY_BASE_ADDRESS); // 只有这个范围内的地址才进行虚拟地址转物理地址
	else
		//lqm masked debug info.
		//printf("The input address don't need "\
		//	"a virtual-to-physical translation : %08lx\n", addr);

	return addr;
}
#else
ulong virt_to_phy_smdkc110(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xd0000000))
		return (addr - 0xc0000000 + 0x30000000);
	else if ((0x30000000 <= addr) && (addr < 0x50000000))
		return addr;
	else
		printf("The input address don't need "\
			"a virtual-to-physical translation : %08lx\n", addr);

	return addr;
}
#endif

#endif

#if defined(CONFIG_CMD_NAND) && defined(CFG_NAND_LEGACY)
#include <linux/mtd/nand.h>
extern struct nand_chip nand_dev_desc[CFG_MAX_NAND_DEVICE];
void nand_init(void)
{
	nand_probe(CFG_NAND_BASE);
        if (nand_dev_desc[0].ChipID != NAND_ChipID_UNKNOWN) {
                print_size(nand_dev_desc[0].totlen, "\n");
        }
}
#endif

//lqm test
/*
void mpadfb_init()
{
	return;
}
*/

/************lxg added***************/
extern void mpadfb_init(void);
int x210_preboot_init(void)
{
	// LCD干活的内容都在这个函数中
	mpadfb_init();
	return 1;
}
/**********end************************/
