#include <common.h>
#include <s5pc110.h>

#include <movi.h>
#include <asm/io.h>
#include <regs.h>
#include <mmc.h>

#if defined(CONFIG_SECURE_BOOT)
#include <secure_boot.h>
#endif

extern raw_area_t raw_area_control;

// 第一个是通道号；第二个是SD卡开始扇区号；第三个是读取扇区的个数；
// 第四个读取到的地址(读取后放入内存的地址)；第五个传参0即可
typedef u32(*copy_sd_mmc_to_mem)
(u32 channel, u32 start_block, u16 block_size, u32 *trg, u32 init);

void movi_bl2_copy(void)
{
	ulong ch;
#if defined(CONFIG_EVT1)
	// 0xD0037488地址中存的值0xEB000000或0xEB200000
	ch = *(volatile u32 *)(0xD0037488);		// 读出0xD0037488地址中的值(指针的解引用)，判断是从SD卡的哪个通道执行的
	copy_sd_mmc_to_mem copy_bl2 =
	    (copy_sd_mmc_to_mem) (*(u32 *) (0xD0037F98));	// 读取SD卡扇区到DDR中

	#if defined(CONFIG_SECURE_BOOT)
	ulong rv;
	#endif
#else
	ch = *(volatile u32 *)(0xD003A508);
	copy_sd_mmc_to_mem copy_bl2 =
	    (copy_sd_mmc_to_mem) (*(u32 *) (0xD003E008));
#endif
	u32 ret;
	if (ch == 0xEB000000) {		// 指针解引用，判断0xD0037488地址中的值是否为0xEB000000(SD0通道)
		ret = copy_bl2(0, MOVI_BL2_POS, MOVI_BL2_BLKCNT,
			CFG_PHY_UBOOT_BASE, 0);		// DDR初始化好之后，整个DDR都可以使用了，将SD卡中整个程序(BL1+BL2)读取到DDR中，我们选择的是CFG_PHY_UBOOT_BASE宏值为33e00000

#if defined(CONFIG_SECURE_BOOT)
		/* do security check */
		rv = Check_Signature( (SecureBoot_CTX *)SECURE_BOOT_CONTEXT_ADDR,
				      (unsigned char *)CFG_PHY_UBOOT_BASE, (1024*512-128),
			              (unsigned char *)(CFG_PHY_UBOOT_BASE+(1024*512-128)), 128 );
		if (rv != 0){
				while(1);
			}
#endif
	}// 0xEB200000指的是SD2通道
	else if (ch == 0xEB200000) {
		ret = copy_bl2(2, MOVI_BL2_POS, MOVI_BL2_BLKCNT,
			CFG_PHY_UBOOT_BASE, 0);		// 读取SD卡中的内容到DDR中
		
#if defined(CONFIG_SECURE_BOOT)
		/* do security check */
		rv = Check_Signature( (SecureBoot_CTX *)SECURE_BOOT_CONTEXT_ADDR,
				      (unsigned char *)CFG_PHY_UBOOT_BASE, (1024*512-128),
			              (unsigned char *)(CFG_PHY_UBOOT_BASE+(1024*512-128)), 128 );
		if (rv != 0) {
			while(1);
		}
#endif
	}
	else
		return;

	if (ret == 0)
		while (1)
			;
	else
		return;
}

/*
 * Copy zImage from SD/MMC to mem
 */
#ifdef CONFIG_MCP_SINGLE
void movi_zImage_copy(void)
{
	copy_sd_mmc_to_mem copy_zImage =
	    (copy_sd_mmc_to_mem) (*(u32 *) COPY_SDMMC_TO_MEM);
	u32 ret;

	/*
	 * 0x3C6FCE is total size of 2GB SD/MMC card
	 * TODO : eMMC will be used as boot device on HP proto2 board
	 *        So, total size of eMMC will be re-defined next board.
	 */
	ret =
	    copy_zImage(0, 0x3C6FCE, MOVI_ZIMAGE_BLKCNT, CFG_PHY_KERNEL_BASE,
			1);

	if (ret == 0)
		while (1)
			;
	else
		return;
}
#endif

void print_movi_bl2_info(void)
{
	printf("%d, %d, %d\n", MOVI_BL2_POS, MOVI_BL2_BLKCNT, MOVI_ENV_BLKCNT);
}

void movi_write_env(ulong addr)		// addr是环境变量在内存中所在的地址
{
	// raw_area_control是uboot中规划iNand/SD卡的原始分区表，这个里面记录了
	// 我们对iNand的分区，env分区也在这里，下标是2。追到这一层就够了。
	// 在里面就是调用驱动部分的写SD卡/iNand的底层函数了。
	movi_write(raw_area_control.image[2].start_blk,
		   raw_area_control.image[2].used_blk, addr);	// addr中环境变量的大小和raw_area_control中扇区的大小，都是在x210_sd.h中定义的
}	// raw_area_control.image[2].start_blk(第一个参数)是起始扇区，raw_area_control.image[2].used_blk(第二个参数)是已使用的扇区

void movi_read_env(ulong addr)
{
	// raw_area_control就是原始信息分区表
	movi_read(raw_area_control.image[2].start_blk,
		  raw_area_control.image[2].used_blk, addr);
}

void movi_write_bl1(ulong addr)
{
	int i;
	ulong checksum;
	ulong src;
	ulong tmp;

	src = addr;
#if defined(CONFIG_EVT1)
	addr += 16;
	for (i = 16, checksum = 0; i < SS_SIZE; i++) {
		checksum += *(u8 *) addr++;
	}
	printf("checksum : 0x%x\n", checksum);
	*(volatile u32 *)(src + 0x8) = checksum;
	movi_write(raw_area_control.image[1].start_blk,
		   raw_area_control.image[1].used_blk, src);
#else
	for (i = 0, checksum = 0; i < SS_SIZE - 4; i++) {
		checksum += *(u8 *) addr++;
	}

	tmp = *(ulong *) addr;
	*(ulong *) addr = checksum;

	movi_write(raw_area_control.image[0].start_blk,
		   raw_area_control.image[0].used_blk, src);

	*(ulong *) addr = tmp;
#endif
}

#if defined(CONFIG_VOGUES)
int movi_boot_src()
{
	ulong reg;
	ulong src;

	reg = (*(volatile u32 *)(INF_REG_BASE + INF_REG3_OFFSET));

	if (reg == BOOT_MMCSD)
		/* boot device is SDMMC */
		src = 0;
	else if (reg == BOOT_NOR)
		/* boot device is NOR */
		src = 1;

	return src;
}
#endif
