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
// ��Ϊ���������������ֳɵ���ȷ�ģ���ֲ��ʱ�������ǲ���Ҫ�Ķ��ģ�
// �ؼ�������Ļ�����ʼ������Ϊ��Щ������ʼ����Ӳ����صġ�
static void dm9000_pre_init(void)		// ������Ҫ��������GPIO�Ͷ˿ڵ����ã�����������
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

// X210��������صĳ�ʼ��
int board_init(void)
{
// ������������Ϊ�˺���ʹ��gd���㡣
// ���Կ�����gd��������һ�����ԭ���������Ҫ����ʹ��gd����˾�Ҫ��������������ɺ�ȽϷ��㡣
	DECLARE_GLOBAL_DATA_PTR;
#ifdef CONFIG_DRIVER_SMC911X
	smc9115_pre_init();
#endif

#ifdef CONFIG_DRIVER_DM9000		// ������������ÿ������������
	// ��������ֲubootʱ�����Ҫ��ֲ��������Ҫ�Ĺ���������
	// ��Ϊ��������д�õģ��ؼ��ľ�������ĳ�ʼ����������Ϊ��ÿ��������Ӳ��(GPIO�Ͷ˿ڵ�����)�й�
	dm9000_pre_init();			// DM9000������ʼ������
#endif
	// bi_arch_number��board_info�е�һ��Ԫ�أ�������: ������Ļ����롣��ν���������uboot�����
	// �����嶨���Ψһ��š����������Ҫ���þ�����uboot��Linux�ں�֮����бȶԺ����䡣
	// Ƕ��ʽ�豸��ÿһ���豸��Ӳ�����Ƕ��ƻ��ģ�����ͨ�á�
	// Ƕ��ʽ�豸�ĸ߶ȶ��ƻ�����Ӳ������������������ʹ�á�
	// ��͸������������������ֲ���ں˾�����Բ������ص���һ��������ȥ������Ҳ��������������
	// ����Ҳ���������������кܶ����������Linux���˸�����: ��ÿ�����������˸�Ψһ���(������)��
	// Ȼ����uboot��Linux�ں��ж���һ�����ά���Ļ������š�Ȼ�󿪷��塢uboot��Linux����ȥ�Ա�
	// �����룬�������������˾�����������Ͳ�����(��Ϊ�����Ϊ�Һ����Ӳ��������)��
	// MACH_TYPE��x210_sd.h�ж��壬ֵ��2456����û�����⺬�壬ֻ�ǵ�ǰ�������Ӧ�ı�š�������
	// �ʹ�����X210���������Ļ����룬�������������������ֲ��Linux�ں��еĻ�����Ҳ������2456��
	// �����������������
	// uboot�����õ���������롣����Ϊuboot��Linux�ں˵Ĵ��ε�һ���ִ���Linux�ںˣ��ں���������
	// �л�Ա�������յ��Ļ����룬���Լ�����Ļ�������Աȣ�������kernel���������������������
	// ��������˵��һ��������Ļ����벻���Լ���㶨��������˵��Ȩ��ȥ��������������ֻ��uboot�ٷ���
	// ������������һ�������岢����ֲ��uboot֮��������Ӧ���ύ��uboot�ٷ���˲����Ż����롣���ǹ���
	// �Ŀ����������û������(��Ҫԭ������Ϊ���ڿ�����Ӣ�Ĳ��У��͹��⿪Դ�����Ӵ��Ƚ���)�������Լ�
	// ����ŵġ�����ŵ���������п��ܺͱ��˵ı�ų�ͻ������ֻҪ��֤uboot��kernel�еı����һ��
	// �ģ��Ͳ�Ӱ���Լ��Ŀ�����������
	gd->bd->bi_arch_number = MACH_TYPE;
	// bi_boot_params��board_info��һ����ҪԪ�أ���ʾuboot��Linux kernel����ʱ�Ĵ��ε��ڴ��ַ��
	// Ҳ����˵uboot��kernel���ε�ʱ������ô����: uboot���Ƚ�׼���õĴ���(�ַ���������bootargs)
	// �����ڴ��һ����ַ��(����bi_boot_params)��Ȼ��uboot���������ں�(uboot�������ں�ʱ������
	// ͨ���Ĵ���r0 r1 r2��ֱ�Ӵ��ݲ����ģ�������һ���Ĵ����о���bi_boot_params)���ں��������
	// �Ĵ����ж�ȡbi_boot_params��֪����uboot���Ҵ��ݵĲ����������ڴ�����Ȼ���Լ�ȥ�ڴ��
	// �Ǹ��ط�ȥ��bootargs��
	// ˵���ˣ�����bootargs�ŵ�bi_boot_params���ڵ��ڴ��У�����ڴ����ʼ��ַ�ŵ�r2�Ĵ�����
	// �ں�ͨ����ȡr2�Ĵ�����bi_boot_params�����ڴ����ʼ��ַ���õ�ͨ��ָ������õķ�������ӵõ�bootargs��
	// ���������֪��bi_boot_params��ֵΪ0x30000100������ڴ��ַ�ͱ������������ڴ洫���ˡ�
	// ������uboot�������ط�ʹ���ڴ�ʱҪע�⣬ǧ�򲻸Ұ��������û�ˡ�
	gd->bd->bi_boot_params = (PHYS_SDRAM_1+0x100);			// 30000100

	return 0;
}

int dram_init(void)
{
	// DDR��ʼ��
	// ע��: ����ĳ�ʼ��DDR�ͻ��׶�lowlevel_init�г�ʼ��DDR�ǲ�ͬ��
	// ��ʱ��Ӳ���ĳ�ʼ����Ŀ������DDR���Կ�ʼ������
	// ����������ṹ��һЩDDR��ص��������á���ַ���õĳ�ʼ�����Ǵ��������ġ�
	// �����γ�ʼ��DDR��ԭ��: ����uboot��˵������ô֪���������ϵ����м�ƬDDR�ڴ棬ÿһƬ
	// ����ʼ��ַ��������Щ��Ϣ��? ��uboot������в�����һ�ּ�ֱ����Ч�ķ�ʽ: ����Ա����ֲ
	// uboot��һ��������ʱ������Ա�Լ���x210_sd.h��ʹ�ú궨��ȥ���ó���������DDR�ڴ����Ϣ��
	// Ȼ��ubootֻҪ��ȡ��Щ��Ϣ���ɡ�(ʵ���ϻ�������һ��˼·: ����ubootͨ�������ȡӲ����Ϣ
	// ��֪��DDR���ã�����ubootû��������ʵ����PC��BIOS���õ�������)
	// dram_init�����ڸ�gd->bd�������DDR���ò��ֵ�ȫ�ֱ�����ֵ����gd->bd���ݼ�¼�µ�ǰ������
	// ��DDR��������Ϣ���Ա�uboot��ʹ���ڴ档
	// �Ӵ�����������ʵ���ǳ�ʼ��gd->bd->bi_dram����ṹ�����顣
	DECLARE_GLOBAL_DATA_PTR;

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;		// ��һƬ�ڴ���ʼ��ַ
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;	// ��һƬ�ڴ�Ĵ�С

#if defined(PHYS_SDRAM_2)
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;		// �ڶ�Ƭ�ڴ����ʼ��ַ
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;	// �ڶ�Ƭ�ڴ�Ĵ�С
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
// ��顢ȷ�Ͽ��������˼��������������þ��Ǽ�鵱ǰ���������ĸ������岢�Ҵ�ӡ�������������
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
		return (addr - 0xc0000000 + MEMORY_BASE_ADDRESS); // ֻ�������Χ�ڵĵ�ַ�Ž��������ַת�����ַ
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
	// LCD�ɻ�����ݶ������������
	mpadfb_init();
	return 1;
}
/**********end************************/
