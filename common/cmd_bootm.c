/*
 * (C) Copyright 2009-2010 Samsung Electronics Co., Ltd.
 *
 * (C) Copyright 2000-2006
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


/*
 * Boot support
 */
#include <common.h>
#include <watchdog.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <zlib.h>
#include <bzlib.h>
#include <environment.h>
#include <lmb.h>
#include <asm/byteorder.h>

#if defined(CONFIG_CMD_USB)
#include <usb.h>
#endif

#if defined(CONFIG_CMD_DATE) || defined(CONFIG_TIMESTAMP)
#include <rtc.h>
#endif

#ifdef CFG_HUSH_PARSER
#include <hush.h>
#endif

#if defined(CONFIG_SECURE_BOOT)
#include <secure_boot.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

extern int gunzip (void *dst, int dstlen, unsigned char *src, unsigned long *lenp);
#ifndef CFG_BOOTM_LEN
#define CFG_BOOTM_LEN	0x800000	/* use 8MByte as default max gunzip size */
#endif

#ifdef CONFIG_BZIP2
extern void bz_internal_error(int);
#endif

#if defined(CONFIG_CMD_IMI)
static int image_info (unsigned long addr);
#endif

#if defined(CONFIG_CMD_IMLS)
#include <flash.h>
extern flash_info_t flash_info[]; /* info for FLASH chips */
static int do_imls (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#endif

static void print_type (image_header_t *hdr);

#ifdef CONFIG_SILENT_CONSOLE
static void fixup_silent_linux (void);
#endif

static image_header_t *image_get_kernel (ulong img_addr, int verify);
#if defined(CONFIG_FIT)
static int fit_check_kernel (const void *fit, int os_noffset, int verify);
#endif

static void *boot_get_kernel (cmd_tbl_t *cmdtp, int flag,int argc, char *argv[],
		bootm_headers_t *images, ulong *os_data, ulong *os_len);
extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

/*
 *  Continue booting an OS image; caller already has:
 *  - copied image header to global variable `header'
 *  - checked header magic number, checksums (both header & image),
 *  - verified image architecture (PPC) and type (KERNEL or MULTI),
 *  - loaded (first part of) image to header load address,
 *  - disabled interrupts.
 */
typedef void boot_os_fn (cmd_tbl_t *cmdtp, int flag,
			int argc, char *argv[],
			bootm_headers_t *images); /* pointers to os/initrd/fdt */

extern boot_os_fn do_bootm_linux;
static boot_os_fn do_bootm_netbsd;
#if defined(CONFIG_LYNXKDI)
static boot_os_fn do_bootm_lynxkdi;
extern void lynxkdi_boot (image_header_t *);
#endif
static boot_os_fn do_bootm_rtems;
#if defined(CONFIG_CMD_ELF)
static boot_os_fn do_bootm_vxworks;
static boot_os_fn do_bootm_qnxelf;
int do_bootvx (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
int do_bootelf (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#endif
#if defined(CONFIG_ARTOS) && defined(CONFIG_PPC)
static boot_os_fn do_bootm_artos;
#endif

ulong load_addr = CFG_LOAD_ADDR;	/* Default Load Address */
static bootm_headers_t images;		/* pointers to os/initrd/fdt images */

void __board_lmb_reserve(struct lmb *lmb)
{
	/* please define platform specific board_lmb_reserve() */
}
void board_lmb_reserve(struct lmb *lmb) __attribute__((weak, alias("__board_lmb_reserve")));


/*******************************************************************/
/* bootm - boot application image from image in memory */
/*******************************************************************/
// uboot的本质就是一个复杂点的裸机程序，和我们在ARM裸机全集中学习的每一个裸机程序并没有本质区别。
// ARM裸机第十六部分谢了简单的shell，这东西其实就是个mini型的uboot。
// 操作系统内核本身就是一个裸机程序、和uboot、和其他裸机程序并没有本质区别。
// 区别就是操作系统运行起来后在软件上分为内核层和应用层，分层后两层的权限不同，内存访问和设备操作
// 的管理上更加精细(内核可以随便访问各种硬件，而应用程序只能被限制的访问硬件和内存地址)
// 直观来看: uboot的镜像是u-boot.bin，Linux系统的镜像是zImage，这两个东西其实都是两个裸机程序镜像。
// 从系统的启动角度来讲，内核其实就是一个大的复杂的裸机程序。

// 一个完整的软件+硬件的嵌入式系统，静止时(未上电时)BootLoader、kernel、rootfs等必须的软件都已镜像
// 的形式存储在启动介质中(X210中是iNand、SD卡)；运行时都是在DDR内存中运行的，与存储介质无关。上面
// 2个状态都是稳定状态，第3个状态是动态过程，，即从静止态到运行态的过程，也就是启动过程。
// 动态启动过程就是一个从SD卡逐步搬移到DDR内存，并且运行启动代码进行相关的硬件初始化和软件架构的建立
// 最终达到运行时稳定状态。
// 静止时u-boot.bin zImage rootfs都在SD卡中，他们不可能随意存在SD卡的任意位置，因此需要对SD卡进行一
// 个分区，然后将各种镜像各自存在各自的分区中，这样在启动过程中uboot、kernel等就知道到哪里去找谁。(
// uboot和kernel中的分区表必须一致，同时和SD卡的实际使用的分区要一致)

// uboot在第一阶段中进行重定位时将第二阶段(整个uboot镜像)加载到DDR的0xc3e00000地址处，这个地址就是
// uboot的链接地址。
// 内核也有类似要求，uboot启动内核时将kernel从SD卡读取放到DDR中(其实就是个重定位的过程)，不能随意放置，
// 必须放在kernel的链接地址处。否则启动不起来。譬如我们使用的内核链接地址是0x30008000。

// uboot是无条件启动的，完全从零开始启动的。
// 内核是不能开机自动完全从零开始启动的，内核启动要别人帮忙。uboot要帮助内核实现重定位(从SD卡到DDR)
// uboot还要给内核提供启动参数。

// uboot要启动内核，分为2个步骤: 第一步是将内核镜像从启动介质中加载到DDR中，第二步是去DDR中启动内核
// 镜像。(内核代码根本就没考虑重定位，因为内核知道会有uboot之类的把自己加载到DDR中链接地址处的，所以
// 内核直接就从链接地址处开始运行的)

// 内核镜像放在哪里?
// 1、常规启动时各种镜像都在SD卡中，因此uboot只需要从SD卡的kernel分区去读取kernel镜像到DDR中(放到链接地址处
// 链接地址去内核源代码的链接脚本或者Makefile中去查找。X210中是0x30008000)即可。读取要使用uboot的命令
// 来读取(譬如x210的iNand版本是movi命令，x210的Nand版本就是Nand命令)，输入movi read kernel 0x30008000
// 这种启动方式来加载DDR，使用movi read kernel 0x30008000。其中kernel指的是uboot中的kernel分区(就是uboot
// 中规定的SD卡中的一个区域范围，这个区域范围被设计来存放kernel镜像，就是所谓的kernel分区)
// 2、uboot还支持远程启动，也就是内核镜像不烧录到开发板的SD卡中，而是放在主机的服务器中，然后需要启动
// 时uboot通过网络从服务器中下载镜像到开发板的DDR中。
// 分析总结: 最终结果要的是内核镜像到DDR中特定地址即可，不管内核镜像是怎么到DDR中的。以上2种方式各有
// 优势。产品出厂时会设置为从SD卡中启动，tftp下载远程启动这种方式一般用来开发。
// bootcmd命令是用来指定用哪种方式启动内核的(有SD卡、tftp下载两种方式)
// bootargs命令是用来指定哪种方式启动rootfs的(nfs挂载、根文件系统烧录两种方式)

// do_bootm函数的核心就是去分辨传进来的image到底是什么类型，然后按照这种类型的头信息格式去校验。
// 校验通过则进入下一步准备启动内核；如果校验失败则认为镜像有问题，所以不能启动内核。

// 总结: uboot本身设计时只支持uImage启动，原来uboot的代码也是这样写的。后来有了fdt方式之后，就把
// uImage方式命名为LEGACY方式，fdt方式命令为FIT方式，于是乎多了些#if #endif添加的代码。后来移植的
// 人又为了省事添加了zImage启动的方式，又为了省事把zImage启动方式直接写在了uImage和fdt启动方式之前，
// 于是乎又有了一对#if #endif。于是乎整个的代码看起来很恶心。
int do_bootm (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	// 定义了一些局部变量
	image_header_t	*hdr;
	ulong		addr;
	ulong		iflag;
	const char	*type_name;
	uint		unc_len = CFG_BOOTM_LEN;
	uint8_t		comp, type, os;

	void		*os_hdr;
	ulong		os_data, os_len;
	ulong		image_start, image_end;
	ulong		load_start, load_end;
	ulong		mem_start;
	phys_size_t	mem_size;

	struct lmb lmb;

#if defined(CONFIG_SECURE_BOOT)
	int rv;
#endif

#if defined(CONFIG_SECURE_BOOT)		// CONFIG_SECURE_BOOT是安全启动，签名认证，不用管它
	rv = Check_Signature( (SecureBoot_CTX *)SECURE_BOOT_CONTEXT_ADDR,
                                (unsigned char*)CONFIG_SECURE_KERNEL_BASE,
                                CONFIG_SECURE_KERNEL_SIZE-128,
                                (unsigned char*)(CONFIG_SECURE_KERNEL_BASE+CONFIG_SECURE_KERNEL_SIZE-128),
                                128 );
        if(rv != SB_OK) {
                printf("Kernel Integrity check fail\nSystem Halt....");
                while(1);
        }
        printf("Kernel Integirty check success.\n");

	rv = Check_Signature( (SecureBoot_CTX *)SECURE_BOOT_CONTEXT_ADDR,
                                (unsigned char*)CONFIG_SECURE_ROOTFS_BASE,
                                CONFIG_SECURE_ROOTFS_SIZE-128,
                                (unsigned char*)(CONFIG_SECURE_ROOTFS_BASE+CONFIG_SECURE_ROOTFS_SIZE-128),
                                128 );
	if(rv != SB_OK) {
                printf("rootfs Integrity check fail\nSystem Halt....");
                while(1);
        }

        printf("rootfs Integirty check success.\n");

#endif
	// 对images结构体先清零，然后填充
	memset ((void *)&images, 0, sizeof (images));
	images.verify = getenv_yesno ("verify");
	images.lmb = &lmb;

	lmb_init(&lmb);

	mem_start = getenv_bootm_low();
	mem_size = getenv_bootm_size();

	lmb_add(&lmb, (phys_addr_t)mem_start, mem_size);

	board_lmb_reserve(&lmb);

// vmlinux和zImage和uImage
// 1、uboot经过编译直接生成的elf格式的可执行程序是u-boot，这个程序类似于windows下的exe格式，在操作系统
// 下是可以直接执行的，但是这种格式不能用来烧录下载。我们用来烧录下载的是u-boot.bin，这个东西是由
// u-boot使用arm-linux-objcopy(这个工具是把elf格式转为可烧录下载的.bin格式)工具进行加工(主要目的是
// 去掉一些无用的)得到的。这个u-boot.bin就叫镜像(image)，镜像就是用来烧录到iNand中执行的。
// 2、Linux内核经过编译后也会生成一个elf格式的可执行程序，叫vmlinux或vmlinuz，这个就是原始的未经任何处理
// 加工的原版内核elf文件；嵌入式系统在部署时烧录的一般不是这个vmlinux或vmlinuz，而是要用objcopy工具
// 去制作成烧录镜像格式(就是u-boot.bin这种，但是内核没有.bin后缀)，经过制作加工成烧录镜像的文件就叫
// Image(制作把78M大的精简成了7.5M，因此这个制作烧录镜像主要目的就是缩减大小，节省磁盘)。
// 原则上Image就可以直接被烧录到Flash上进行启动执行(类似于u-boot.bin)，但是实际上并不是这么简单。实际
// 上Linux的作者们觉得Image还是太大了，所以对Image进行了压缩，并且在image压缩后的文件的前段附加了一部分
// 解压缩代码。构成了一个压缩格式的镜像就叫zImage。(因为当年Image大小刚好比一张软盘(软盘有两种，1.2M
// 和1.44M两种)大，为了节省一张软盘的钱，于是乎设计了这种压缩Image成zImage的技术)
// uboot为了启动Linux内核，还发明了一种内核格式叫uImage。uImage是由zImage加工得到的，uboot中有一个
// 工具，可以将zImage加工生成uImage。注意: uImage不关Linux内核的事，Linux内核只管生成zImage即可，然后
// uboot中的mkimage工具去由zImage加工生成uImage来给uboot启动。这个加工过程其实就是在zImage前面加上64
// 字节的uImage的头信息即可。
// 原则上uboot启动时应该给它uImage格式的内核镜像，但是实际上uboot中也可以支持zImage，是否支持就看
// x210_sd.h中是否定义了CONFIG_ZIMAGE_BOOT这个宏。所以大家可以看出: 有些uboot是支持zImage启动的，有些
// 则不支持。但是所有的uboot肯定都支持uImage启动。

// 这个宏来控制进行条件编译一段代码，这段代码是用来支持zImage格式的内核启动的
#ifdef CONFIG_ZIMAGE_BOOT
// 这个是一个定义的魔数，表示镜像是zImage。zImage格式的镜像中在头部的一个固定位置存放了这个数作为标记
// 如果我们拿到一个image，去它的那个位置去取4字节判断它是否等于LINUX_ZIMAGE_MAGIC，则可以知道这个镜像
// 是不是zImage。
#define LINUX_ZIMAGE_MAGIC	0x016f2818
// 命令执行 bootm 0x30008000，所以do_bootm的argc=2，argv[0]=bootm，argv[1]=0x30008000
	/* find out kernel image address */
	// 1、对启动命令进行判断
	// bootm不带参数，直接bootm，则会从CFG_LOAD_ADDR地址去执行
	if (argc < 2) {		
		addr = load_addr;
		debug ("*  kernel: default image load address = 0x%08lx\n",
				load_addr);
	} else {
	// bootm带参数: bootm 0x30008000；do_bootm的argc=2，argv[0]=bootm，argv[1]=0x30008000
	// addr是内核启动的在DDR中的首地址
		addr = simple_strtoul(argv[1], NULL, 16);
		debug ("*  kernel: cmdline image address = 0x%08lx\n", img_addr);
	}

	// 2、对启动镜像的标志魔数进行判断
	// zImage头部开始的第36-39字节处存放着zImage标志魔数，从这个位置取出与LINUX_ZIMAGE_MAGIC对比
	// 可以用过二进制阅读软件来打开zImage查看，就可以证明。譬如winhex软件
	if (*(ulong *)(addr + 9*4) == LINUX_ZIMAGE_MAGIC) {
		printf("Boot with zImage\n");
		// 将虚拟地址转换为物理地址
		addr = virt_to_phys(addr);
		// 把转换为的物理地址强制类型转换为image_header_t *类型，实际是在构建image_header_t结构体
		hdr = (image_header_t *)addr;
		// 对结构体中的两个元素进行赋值
		hdr->ih_os = IH_OS_LINUX;
		hdr->ih_ep = ntohl(addr);
		// 把hdr结构体中想要的元素封装好了之后，再移动到images结构体的相应变量中
		memmove (&images.legacy_hdr_os_copy, hdr, sizeof(image_header_t));
		// 最后构建一个完整的images，然后通过images来启动kernel
		// images全局变量是do_bootm函数中使用，用来完成启动过程的。
		// zImage的校验过程其实就是先确认是不是zImage，确认后再修改zImage的头信息到合适，
		// 修改后用头信息去初始化images这个全局变量，然后就完成了校验。
		/* save pointer to image header */
		images.legacy_hdr_os = hdr;

		images.legacy_hdr_valid = 1;
		// 如果校验完是zImage，则直接跳转到after_header_check，不需要在进行uImage校验了
		goto after_header_check;
	}
#endif
	// uImage的启动校验主要在boot_get_kernel函数中，主要任务就是校验uImage的头信息，并且得到真正的kernel的起始位置启动
	// boot_get_kernel函数的返回值是内核的起始启动位置(在内存中的地址)，然后赋值给局部变量os_hdr
	/* get kernel image header, start address and length */
	os_hdr = boot_get_kernel (cmdtp, flag, argc, argv,
			&images, &os_data, &os_len);
	if (os_len == 0) {
		puts ("ERROR: can't get kernel image!\n");
		return 1;
	}

	/* get image parameters */
	switch (genimg_get_format (os_hdr)) {
	// LEGACY(遗留的)，在do_bootm函数中，这种方式指的是uImage的方式。
	// uImage方式是uboot本身发明的支持Linux启动的镜像方式，但是后来这种方式被一种新的方式替代了，
	// 这个新的方式就是设备树方式(在do_bootm方式叫FIT)
	case IMAGE_FORMAT_LEGACY:
		// 通过数据结构得到uImage镜像的信息
		type = image_get_type (os_hdr);
		comp = image_get_comp (os_hdr);
		os = image_get_os (os_hdr);

		image_end = image_get_image_end (os_hdr);
		load_start = image_get_load (os_hdr);
		break;
#if defined(CONFIG_FIT)
// 设备树主要是用来做内核传参的
	case IMAGE_FORMAT_FIT:
		if (fit_image_get_type (images.fit_hdr_os,
					images.fit_noffset_os, &type)) {
			puts ("Can't get image type!\n");
			show_boot_progress (-109);
			return 1;
		}

		if (fit_image_get_comp (images.fit_hdr_os,
					images.fit_noffset_os, &comp)) {
			puts ("Can't get image compression!\n");
			show_boot_progress (-110);
			return 1;
		}

		if (fit_image_get_os (images.fit_hdr_os,
					images.fit_noffset_os, &os)) {
			puts ("Can't get image OS!\n");
			show_boot_progress (-111);
			return 1;
		}

		image_end = fit_get_end (images.fit_hdr_os);

		if (fit_image_get_load (images.fit_hdr_os, images.fit_noffset_os,
					&load_start)) {
			puts ("Can't get image load address!\n");
			show_boot_progress (-112);
			return 1;
		}
		break;
#endif
	default:
		puts ("ERROR: unknown image format type!\n");
		return 1;
	}

	image_start = (ulong)os_hdr;
	load_end = 0;
	type_name = genimg_get_type_name (type);

	/*
	 * We have reached the point of no return: we are going to
	 * overwrite all exception vector code, so we cannot easily
	 * recover from any failures any more...
	 */
	iflag = disable_interrupts();

#if defined(CONFIG_CMD_USB)
	/*
	 * turn off USB to prevent the host controller from writing to the
	 * SDRAM while Linux is booting. This could happen (at least for OHCI
	 * controller), because the HCCA (Host Controller Communication Area)
	 * lies within the SDRAM and the host controller writes continously to
	 * this area (as busmaster!). The HccaFrameNumber is for example
	 * updated every 1 ms within the HCCA structure in SDRAM! For more
	 * details see the OpenHCI specification.
	 */
	usb_stop();
#endif


#ifdef CONFIG_AMIGAONEG3SE
	/*
	 * We've possible left the caches enabled during
	 * bios emulation, so turn them off again
	 */
	icache_disable();
	invalidate_l1_instruction_cache();
	flush_data_cache();
	dcache_disable();
#endif
	// comp局部变量表示的压缩格式
	switch (comp) {
	// 我们的代码走的是这个流程；comp=IH_COMP_NONE
	case IH_COMP_NONE:
		if (load_start == (ulong)os_hdr) {
			printf ("   XIP %s ... ", type_name);
		} else {
			printf ("   Loading %s ... ", type_name);

			memmove_wd ((void *)load_start,
				   (void *)os_data, os_len, CHUNKSZ);
		}
		load_end = load_start + os_len;
		puts("OK\n");
		break;
	// 如果是comp=IH_COMP_GZIP，则是GZIP压缩格式
	case IH_COMP_GZIP:
		printf ("   Uncompressing %s ... ", type_name);
		if (gunzip ((void *)load_start, unc_len,
					(uchar *)os_data, &os_len) != 0) {
			puts ("GUNZIP: uncompress or overwrite error "
				"- must RESET board to recover\n");
			show_boot_progress (-6);
			do_reset (cmdtp, flag, argc, argv);
		}

		load_end = load_start + os_len;
		break;
#ifdef CONFIG_BZIP2
	// 如果是comp=IH_COMP_BZIP2，则是BZIP2压缩格式
	case IH_COMP_BZIP2:
		printf ("   Uncompressing %s ... ", type_name);
		/*
		 * If we've got less than 4 MB of malloc() space,
		 * use slower decompression algorithm which requires
		 * at most 2300 KB of memory.
		 */
		int i = BZ2_bzBuffToBuffDecompress ((char*)load_start,
					&unc_len, (char *)os_data, os_len,
					CFG_MALLOC_LEN < (4096 * 1024), 0);
		if (i != BZ_OK) {
			printf ("BUNZIP2: uncompress or overwrite error %d "
				"- must RESET board to recover\n", i);
			show_boot_progress (-6);
			do_reset (cmdtp, flag, argc, argv);
		}

		load_end = load_start + unc_len;
		break;
#endif /* CONFIG_BZIP2 */
	default:
		if (iflag)
			enable_interrupts();
		printf ("Unimplemented compression type %d\n", comp);
		show_boot_progress (-7);
		return 1;
	}
	puts ("OK\n");
	debug ("   kernel loaded at 0x%08lx, end = 0x%08lx\n", load_start, load_end);
	show_boot_progress (7);

	if ((load_start < image_end) && (load_end > image_start)) {
		debug ("image_start = 0x%lX, image_end = 0x%lx\n", image_start, image_end);
		debug ("load_start = 0x%lx, load_end = 0x%lx\n", load_start, load_end);

		if (images.legacy_hdr_valid) {
			if (image_get_type (&images.legacy_hdr_os_copy) == IH_TYPE_MULTI)
				puts ("WARNING: legacy format multi component "
					"image overwritten\n");
		} else {
			puts ("ERROR: new format image overwritten - "
				"must RESET the board to recover\n");
			show_boot_progress (-113);
			do_reset (cmdtp, flag, argc, argv);
		}
	}

	show_boot_progress (8);

	lmb_reserve(&lmb, load_start, (load_end - load_start));

#if defined(CONFIG_ZIMAGE_BOOT)
after_header_check:
	os = hdr->ih_os;
#endif				// do_bootm函数中一直到这里，之前的代码都是在进行镜像的头部信息校验
					// 校验时就要根据不同种类的image类型进行不同的校验。
	// uboot支持多种内核启动，我们这里走的是Linux kernel
	switch (os) {
	default:			/* handled by (original) Linux case */
	case IH_OS_LINUX:
#ifdef CONFIG_SILENT_CONSOLE
	    fixup_silent_linux();
#endif
		// 我们之前在三种镜像校验之后，都封装了images结构体，目的就是在这里当传参来使用
		// images全局变量的作用是把镜像启动所需要的有效信息存储到这里
		// 调用do_bootm_linux函数时将images作为传参
	    do_bootm_linux (cmdtp, flag, argc, argv, &images);
	    break;

	case IH_OS_NETBSD:
	    do_bootm_netbsd (cmdtp, flag, argc, argv, &images);
	    break;

#ifdef CONFIG_LYNXKDI
	case IH_OS_LYNXOS:
	    do_bootm_lynxkdi (cmdtp, flag, argc, argv, &images);
	    break;
#endif

	case IH_OS_RTEMS:
	    do_bootm_rtems (cmdtp, flag, argc, argv, &images);
	    break;

#if defined(CONFIG_CMD_ELF)
	case IH_OS_VXWORKS:
	    do_bootm_vxworks (cmdtp, flag, argc, argv, &images);
	    break;

	case IH_OS_QNX:
	    do_bootm_qnxelf (cmdtp, flag, argc, argv, &images);
	    break;
#endif

#ifdef CONFIG_ARTOS
	case IH_OS_ARTOS:
	    do_bootm_artos (cmdtp, flag, argc, argv, &images);
	    break;
#endif
	}

	show_boot_progress (-9);
#ifdef DEBUG
	puts ("\n## Control returned to monitor - resetting...\n");
	do_reset (cmdtp, flag, argc, argv);
#endif
	if (iflag)
		enable_interrupts();

	return 1;
}

/**
 * image_get_kernel - verify legacy format kernel image
 * @img_addr: in RAM address of the legacy format image to be verified
 * @verify: data CRC verification flag
 *
 * image_get_kernel() verifies legacy image integrity and returns pointer to
 * legacy image header if image verification was completed successfully.
 *
 * returns:
 *     pointer to a legacy image header if valid image was found
 *     otherwise return NULL
 */
static image_header_t *image_get_kernel (ulong img_addr, int verify)
{
	image_header_t *hdr = (image_header_t *)img_addr;
	// 检查magic
	if (!image_check_magic(hdr)) {
		puts ("Bad Magic Number\n");
		show_boot_progress (-1);
		return NULL;
	}
	show_boot_progress (2);

	if (!image_check_hcrc (hdr)) {
		puts ("Bad Header Checksum\n");
		show_boot_progress (-2);
		return NULL;
	}

	show_boot_progress (3);
	// 打印uImage信息
	image_print_contents (hdr);

	if (verify) {
		puts ("   Verifying Checksum ... ");
		if (!image_check_dcrc (hdr)) {
			printf ("Bad Data CRC\n");
			show_boot_progress (-3);
			return NULL;
		}
		puts ("OK\n");
	}
	show_boot_progress (4);

	if (!image_check_target_arch (hdr)) {
		printf ("Unsupported Architecture 0x%x\n", image_get_arch (hdr));
		show_boot_progress (-4);
		return NULL;
	}
	return hdr;
}

/**
 * fit_check_kernel - verify FIT format kernel subimage
 * @fit_hdr: pointer to the FIT image header
 * os_noffset: kernel subimage node offset within FIT image
 * @verify: data CRC verification flag
 *
 * fit_check_kernel() verifies integrity of the kernel subimage and from
 * specified FIT image.
 *
 * returns:
 *     1, on success
 *     0, on failure
 */
#if defined (CONFIG_FIT)
static int fit_check_kernel (const void *fit, int os_noffset, int verify)
{
	fit_image_print (fit, os_noffset, "   ");

	if (verify) {
		puts ("   Verifying Hash Integrity ... ");
		if (!fit_image_check_hashes (fit, os_noffset)) {
			puts ("Bad Data Hash\n");
			show_boot_progress (-104);
			return 0;
		}
		puts ("OK\n");
	}
	show_boot_progress (105);

	if (!fit_image_check_target_arch (fit, os_noffset)) {
		puts ("Unsupported Architecture\n");
		show_boot_progress (-105);
		return 0;
	}

	show_boot_progress (106);
	if (!fit_image_check_type (fit, os_noffset, IH_TYPE_KERNEL)) {
		puts ("Not a kernel image\n");
		show_boot_progress (-106);
		return 0;
	}

	show_boot_progress (107);
	return 1;
}
#endif /* CONFIG_FIT */

/**
 * boot_get_kernel - find kernel image
 * @os_data: pointer to a ulong variable, will hold os data start address
 * @os_len: pointer to a ulong variable, will hold os data length
 *
 * boot_get_kernel() tries to find a kernel image, verifies its integrity
 * and locates kernel data.
 *
 * returns:
 *     pointer to image header if valid image was found, plus kernel start
 *     address and length, otherwise NULL
 */
// 这个函数就是构建uImage/FIT/其他镜像的启动方式，
// 先对启动命令的参数进行校验，然后分别校验各自镜像的头信息，封装各自镜像的images结构体，
// 获得启动相应镜像的资格
static void *boot_get_kernel (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[],
		bootm_headers_t *images, ulong *os_data, ulong *os_len)
{
	// 这个是uImage镜像的头信息所在的内存地址位置
	image_header_t	*hdr;
	// img_addr这个局部变量指的是镜像所在的内存地址的首地址处
	ulong		img_addr;
#if defined(CONFIG_FIT)
	// fit方式的镜像的头信息所在的内存地址位置
	void		*fit_hdr;
	const char	*fit_uname_config = NULL;
	const char	*fit_uname_kernel = NULL;
	const void	*data;
	size_t		len;
	int		cfg_noffset;
	int		os_noffset;
#endif

	/* find out kernel image address */
	// 这块和zImage的校验步骤是一样的
	if (argc < 2) {
		img_addr = load_addr;
		debug ("*  kernel: default image load address = 0x%08lx\n",
				load_addr);
#if defined(CONFIG_FIT)
	} else if (fit_parse_conf (argv[1], load_addr, &img_addr,
							&fit_uname_config)) {
		debug ("*  kernel: config '%s' from image at 0x%08lx\n",
				fit_uname_config, img_addr);
	} else if (fit_parse_subimage (argv[1], load_addr, &img_addr,
							&fit_uname_kernel)) {
		debug ("*  kernel: subimage '%s' from image at 0x%08lx\n",
				fit_uname_kernel, img_addr);
#endif
	}
	// uImage的启动走这条路
	else {
		img_addr = simple_strtoul(argv[1], NULL, 16);
		debug ("*  kernel: cmdline image address = 0x%08lx\n", img_addr);
	}

	show_boot_progress (1);		// 用来打印启动阶段，来判断错误在哪里

	/* copy from dataflash if needed */
	img_addr = genimg_get_image (img_addr);

	/* check image type, for FIT images get FIT kernel node */
	*os_data = *os_len = 0;
	switch (genimg_get_format ((void *)img_addr)) {
	case IMAGE_FORMAT_LEGACY:
		printf ("## Booting kernel from Legacy Image at %08lx ...\n",
				img_addr);
		hdr = image_get_kernel (img_addr, images->verify);
		if (!hdr)
			return NULL;
		show_boot_progress (5);

		/* get os_data and os_len */
		// image_get_type函数来检测镜像的类型，因为hdr结构体中包含了镜像的类型
		switch (image_get_type (hdr)) {
		// 说明镜像是内核镜像
		case IH_TYPE_KERNEL:
			*os_data = image_get_data (hdr);
			*os_len = image_get_data_size (hdr);
			break;
		case IH_TYPE_MULTI:
			image_multi_getimg (hdr, 0, os_data, os_len);
			break;
		default:
			printf ("Wrong Image Type for %s command\n", cmdtp->name);
			show_boot_progress (-5);
			return NULL;
		}

		/*
		 * copy image header to allow for image overwrites during kernel
		 * decompression.
		 */
		// 这里和zImage时是一样的，将封装好的hdr结构体中的信息移动到images结构体中
		// 也就是在构建images结构体全局变量。
		memmove (&images->legacy_hdr_os_copy, hdr, sizeof(image_header_t));

		/* save pointer to image header */
		images->legacy_hdr_os = hdr;

		images->legacy_hdr_valid = 1;
		show_boot_progress (6);
		break;
// 下面就是构建设备树方式
#if defined(CONFIG_FIT)
	case IMAGE_FORMAT_FIT:
		fit_hdr = (void *)img_addr;
		printf ("## Booting kernel from FIT Image at %08lx ...\n",
				img_addr);

		if (!fit_check_format (fit_hdr)) {
			puts ("Bad FIT kernel image format!\n");
			show_boot_progress (-100);
			return NULL;
		}
		show_boot_progress (100);

		if (!fit_uname_kernel) {
			/*
			 * no kernel image node unit name, try to get config
			 * node first. If config unit node name is NULL
			 * fit_conf_get_node() will try to find default config node
			 */
			show_boot_progress (101);
			cfg_noffset = fit_conf_get_node (fit_hdr, fit_uname_config);
			if (cfg_noffset < 0) {
				show_boot_progress (-101);
				return NULL;
			}
			/* save configuration uname provided in the first
			 * bootm argument
			 */
			images->fit_uname_cfg = fdt_get_name (fit_hdr, cfg_noffset, NULL);
			printf ("   Using '%s' configuration\n", images->fit_uname_cfg);
			show_boot_progress (103);

			os_noffset = fit_conf_get_kernel_node (fit_hdr, cfg_noffset);
			fit_uname_kernel = fit_get_name (fit_hdr, os_noffset, NULL);
		} else {
			/* get kernel component image node offset */
			show_boot_progress (102);
			os_noffset = fit_image_get_node (fit_hdr, fit_uname_kernel);
		}
		if (os_noffset < 0) {
			show_boot_progress (-103);
			return NULL;
		}

		printf ("   Trying '%s' kernel subimage\n", fit_uname_kernel);

		show_boot_progress (104);
		if (!fit_check_kernel (fit_hdr, os_noffset, images->verify))
			return NULL;

		/* get kernel image data address and length */
		if (fit_image_get_data (fit_hdr, os_noffset, &data, &len)) {
			puts ("Could not find kernel subimage data!\n");
			show_boot_progress (-107);
			return NULL;
		}
		show_boot_progress (108);

		*os_len = len;
		*os_data = (ulong)data;
		images->fit_hdr_os = fit_hdr;
		images->fit_uname_os = fit_uname_kernel;
		images->fit_noffset_os = os_noffset;
		break;
#endif
	default:
		printf ("Wrong Image Format for %s command\n", cmdtp->name);
		show_boot_progress (-108);
		return NULL;
	}

	debug ("   kernel data at 0x%08lx, len = 0x%08lx (%ld)\n",
			*os_data, *os_len, *os_len);

	return (void *)img_addr;
}

U_BOOT_CMD(
	bootm,	CFG_MAXARGS,	1,	do_bootm,
	"bootm   - boot application image from memory\n",
	"[addr [arg ...]]\n    - boot application image stored in memory\n"
	"\tpassing arguments 'arg ...'; when booting a Linux kernel,\n"
	"\t'arg' can be the address of an initrd image\n"
#if defined(CONFIG_OF_LIBFDT)
	"\tWhen booting a Linux kernel which requires a flat device-tree\n"
	"\ta third argument is required which is the address of the\n"
	"\tdevice-tree blob. To boot that kernel without an initrd image,\n"
	"\tuse a '-' for the second argument. If you do not pass a third\n"
	"\ta bd_info struct will be passed instead\n"
#endif
#if defined(CONFIG_FIT)
	"\t\nFor the new multi component uImage format (FIT) addresses\n"
	"\tmust be extened to include component or configuration unit name:\n"
	"\taddr:<subimg_uname> - direct component image specification\n"
	"\taddr#<conf_uname>   - configuration specification\n"
	"\tUse iminfo command to get the list of existing component\n"
	"\timages and configurations.\n"
#endif
);

/*******************************************************************/
/* bootd - boot default image */
/*******************************************************************/
#if defined(CONFIG_CMD_BOOTD)
int do_bootd (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int rcode = 0;

#ifndef CFG_HUSH_PARSER
	if (run_command (getenv ("bootcmd"), flag) < 0)
		rcode = 1;
#else
	if (parse_string_outer (getenv ("bootcmd"),
			FLAG_PARSE_SEMICOLON | FLAG_EXIT_FROM_LOOP) != 0)
		rcode = 1;
#endif
	return rcode;
}

U_BOOT_CMD(
	boot,	1,	1,	do_bootd,
	"boot    - boot default, i.e., run 'bootcmd'\n",
	NULL
);

/* keep old command name "bootd" for backward compatibility */
U_BOOT_CMD(
	bootd, 1,	1,	do_bootd,
	"bootd   - boot default, i.e., run 'bootcmd'\n",
	NULL
);

#endif


/*******************************************************************/
/* iminfo - print header info for a requested image */
/*******************************************************************/
#if defined(CONFIG_CMD_IMI)
int do_iminfo (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int	arg;
	ulong	addr;
	int	rcode = 0;

	if (argc < 2) {
		return image_info (load_addr);
	}

	for (arg = 1; arg < argc; ++arg) {
		addr = simple_strtoul (argv[arg], NULL, 16);
		if (image_info (addr) != 0)
			rcode = 1;
	}
	return rcode;
}

static int image_info (ulong addr)
{
	void *hdr = (void *)addr;

	printf ("\n## Checking Image at %08lx ...\n", addr);

	switch (genimg_get_format (hdr)) {
	case IMAGE_FORMAT_LEGACY:
		puts ("   Legacy image found\n");
		if (!image_check_magic (hdr)) {
			puts ("   Bad Magic Number\n");
			return 1;
		}

		if (!image_check_hcrc (hdr)) {
			puts ("   Bad Header Checksum\n");
			return 1;
		}

		image_print_contents (hdr);

		puts ("   Verifying Checksum ... ");
		if (!image_check_dcrc (hdr)) {
			puts ("   Bad Data CRC\n");
			return 1;
		}
		puts ("OK\n");
		return 0;
#if defined(CONFIG_FIT)
	case IMAGE_FORMAT_FIT:
		puts ("   FIT image found\n");

		if (!fit_check_format (hdr)) {
			puts ("Bad FIT image format!\n");
			return 1;
		}

		fit_print_contents (hdr);
		return 0;
#endif
	default:
		puts ("Unknown image format!\n");
		break;
	}

	return 1;
}

U_BOOT_CMD(
	iminfo,	CFG_MAXARGS,	1,	do_iminfo,
	"iminfo  - print header information for application image\n",
	"addr [addr ...]\n"
	"    - print header information for application image starting at\n"
	"      address 'addr' in memory; this includes verification of the\n"
	"      image contents (magic number, header and payload checksums)\n"
);
#endif


/*******************************************************************/
/* imls - list all images found in flash */
/*******************************************************************/
#if defined(CONFIG_CMD_IMLS)
int do_imls (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	flash_info_t *info;
	int i, j;
	void *hdr;

	for (i = 0, info = &flash_info[0];
		i < CFG_MAX_FLASH_BANKS; ++i, ++info) {

		if (info->flash_id == FLASH_UNKNOWN)
			goto next_bank;
		for (j = 0; j < info->sector_count; ++j) {

			hdr = (void *)info->start[j];
			if (!hdr)
				goto next_sector;

			switch (genimg_get_format (hdr)) {
			case IMAGE_FORMAT_LEGACY:
				if (!image_check_hcrc (hdr))
					goto next_sector;

				printf ("Legacy Image at %08lX:\n", (ulong)hdr);
				image_print_contents (hdr);

				puts ("   Verifying Checksum ... ");
				if (!image_check_dcrc (hdr)) {
					puts ("Bad Data CRC\n");
				} else {
					puts ("OK\n");
				}
				break;
#if defined(CONFIG_FIT)
			case IMAGE_FORMAT_FIT:
				if (!fit_check_format (hdr))
					goto next_sector;

				printf ("FIT Image at %08lX:\n", (ulong)hdr);
				fit_print_contents (hdr);
				break;
#endif
			default:
				goto next_sector;
			}

next_sector:		;
		}
next_bank:	;
	}

	return (0);
}

U_BOOT_CMD(
	imls,	1,		1,	do_imls,
	"imls    - list all images found in flash\n",
	"\n"
	"    - Prints information about all images found at sector\n"
	"      boundaries in flash.\n"
);
#endif

/*******************************************************************/
/* helper routines */
/*******************************************************************/
#ifdef CONFIG_SILENT_CONSOLE
static void fixup_silent_linux ()
{
	char buf[256], *start, *end;
	char *cmdline = getenv ("bootargs");

	/* Only fix cmdline when requested */
	if (!(gd->flags & GD_FLG_SILENT))
		return;

	debug ("before silent fix-up: %s\n", cmdline);
	if (cmdline) {
		if ((start = strstr (cmdline, "console=")) != NULL) {
			end = strchr (start, ' ');
			strncpy (buf, cmdline, (start - cmdline + 8));
			if (end)
				strcpy (buf + (start - cmdline + 8), end);
			else
				buf[start - cmdline + 8] = '\0';
		} else {
			strcpy (buf, cmdline);
			strcat (buf, " console=");
		}
	} else {
		strcpy (buf, "console=");
	}

	setenv ("bootargs", buf);
	debug ("after silent fix-up: %s\n", buf);
}
#endif /* CONFIG_SILENT_CONSOLE */


/*******************************************************************/
/* OS booting routines */
/*******************************************************************/

static void do_bootm_netbsd (cmd_tbl_t *cmdtp, int flag,
			    int argc, char *argv[],
			    bootm_headers_t *images)
{
	void (*loader)(bd_t *, image_header_t *, char *, char *);
	image_header_t *os_hdr, *hdr;
	ulong kernel_data, kernel_len;
	char *consdev;
	char *cmdline;

#if defined(CONFIG_FIT)
	if (!images->legacy_hdr_valid) {
		fit_unsupported_reset ("NetBSD");
		do_reset (cmdtp, flag, argc, argv);
	}
#endif
	hdr = images->legacy_hdr_os;

	/*
	 * Booting a (NetBSD) kernel image
	 *
	 * This process is pretty similar to a standalone application:
	 * The (first part of an multi-) image must be a stage-2 loader,
	 * which in turn is responsible for loading & invoking the actual
	 * kernel.  The only differences are the parameters being passed:
	 * besides the board info strucure, the loader expects a command
	 * line, the name of the console device, and (optionally) the
	 * address of the original image header.
	 */
	os_hdr = NULL;
	if (image_check_type (&images->legacy_hdr_os_copy, IH_TYPE_MULTI)) {
		image_multi_getimg (hdr, 1, &kernel_data, &kernel_len);
		if (kernel_len)
			os_hdr = hdr;
	}

	consdev = "";
#if   defined (CONFIG_8xx_CONS_SMC1)
	consdev = "smc1";
#elif defined (CONFIG_8xx_CONS_SMC2)
	consdev = "smc2";
#elif defined (CONFIG_8xx_CONS_SCC2)
	consdev = "scc2";
#elif defined (CONFIG_8xx_CONS_SCC3)
	consdev = "scc3";
#endif

	if (argc > 2) {
		ulong len;
		int   i;

		for (i = 2, len = 0; i < argc; i += 1)
			len += strlen (argv[i]) + 1;
		cmdline = malloc (len);

		for (i = 2, len = 0; i < argc; i += 1) {
			if (i > 2)
				cmdline[len++] = ' ';
			strcpy (&cmdline[len], argv[i]);
			len += strlen (argv[i]);
		}
	} else if ((cmdline = getenv ("bootargs")) == NULL) {
		cmdline = "";
	}

	loader = (void (*)(bd_t *, image_header_t *, char *, char *))image_get_ep (hdr);

	printf ("## Transferring control to NetBSD stage-2 loader (at address %08lx) ...\n",
		(ulong)loader);

	show_boot_progress (15);

	/*
	 * NetBSD Stage-2 Loader Parameters:
	 *   r3: ptr to board info data
	 *   r4: image address
	 *   r5: console device
	 *   r6: boot args string
	 */
	(*loader) (gd->bd, os_hdr, consdev, cmdline);
}

#ifdef CONFIG_LYNXKDI
static void do_bootm_lynxkdi (cmd_tbl_t *cmdtp, int flag,
			     int argc, char *argv[],
			     bootm_headers_t *images)
{
	image_header_t *hdr = &images->legacy_hdr_os_copy;

#if defined(CONFIG_FIT)
	if (!images->legacy_hdr_valid) {
		fit_unsupported_reset ("Lynx");
		do_reset (cmdtp, flag, argc, argv);
	}
#endif

	lynxkdi_boot ((image_header_t *)hdr);
}
#endif /* CONFIG_LYNXKDI */

static void do_bootm_rtems (cmd_tbl_t *cmdtp, int flag,
			   int argc, char *argv[],
			   bootm_headers_t *images)
{
	image_header_t *hdr = &images->legacy_hdr_os_copy;
	void (*entry_point)(bd_t *);

#if defined(CONFIG_FIT)
	if (!images->legacy_hdr_valid) {
		fit_unsupported_reset ("RTEMS");
		do_reset (cmdtp, flag, argc, argv);
	}
#endif

	entry_point = (void (*)(bd_t *))image_get_ep (hdr);

	printf ("## Transferring control to RTEMS (at address %08lx) ...\n",
		(ulong)entry_point);

	show_boot_progress (15);

	/*
	 * RTEMS Parameters:
	 *   r3: ptr to board info data
	 */
	(*entry_point)(gd->bd);
}

#if defined(CONFIG_CMD_ELF)
static void do_bootm_vxworks (cmd_tbl_t *cmdtp, int flag,
			     int argc, char *argv[],
			     bootm_headers_t *images)
{
	char str[80];
	image_header_t *hdr = &images->legacy_hdr_os_copy;

#if defined(CONFIG_FIT)
	if (!images->legacy_hdr_valid) {
		fit_unsupported_reset ("VxWorks");
		do_reset (cmdtp, flag, argc, argv);
	}
#endif

	sprintf(str, "%x", image_get_ep (hdr)); /* write entry-point into string */
	setenv("loadaddr", str);
	do_bootvx(cmdtp, 0, 0, NULL);
}

static void do_bootm_qnxelf(cmd_tbl_t *cmdtp, int flag,
			    int argc, char *argv[],
			    bootm_headers_t *images)
{
	char *local_args[2];
	char str[16];
	image_header_t *hdr = &images->legacy_hdr_os_copy;

#if defined(CONFIG_FIT)
	if (!images->legacy_hdr_valid) {
		fit_unsupported_reset ("QNX");
		do_reset (cmdtp, flag, argc, argv);
	}
#endif

	sprintf(str, "%x", image_get_ep (hdr)); /* write entry-point into string */
	local_args[0] = argv[0];
	local_args[1] = str;	/* and provide it via the arguments */
	do_bootelf(cmdtp, 0, 2, local_args);
}
#endif

#if defined(CONFIG_ARTOS) && defined(CONFIG_PPC)
static void do_bootm_artos (cmd_tbl_t *cmdtp, int flag,
			   int argc, char *argv[],
			   bootm_headers_t *images)
{
	ulong top;
	char *s, *cmdline;
	char **fwenv, **ss;
	int i, j, nxt, len, envno, envsz;
	bd_t *kbd;
	void (*entry)(bd_t *bd, char *cmdline, char **fwenv, ulong top);
	image_header_t *hdr = &images->legacy_hdr_os_copy;

#if defined(CONFIG_FIT)
	if (!images->legacy_hdr_valid) {
		fit_unsupported_reset ("ARTOS");
		do_reset (cmdtp, flag, argc, argv);
	}
#endif

	/*
	 * Booting an ARTOS kernel image + application
	 */

	/* this used to be the top of memory, but was wrong... */
#ifdef CONFIG_PPC
	/* get stack pointer */
	asm volatile ("mr %0,1" : "=r"(top) );
#endif
	debug ("## Current stack ends at 0x%08lX ", top);

	top -= 2048;		/* just to be sure */
	if (top > CFG_BOOTMAPSZ)
		top = CFG_BOOTMAPSZ;
	top &= ~0xF;

	debug ("=> set upper limit to 0x%08lX\n", top);

	/* first check the artos specific boot args, then the linux args*/
	if ((s = getenv( "abootargs")) == NULL && (s = getenv ("bootargs")) == NULL)
		s = "";

	/* get length of cmdline, and place it */
	len = strlen (s);
	top = (top - (len + 1)) & ~0xF;
	cmdline = (char *)top;
	debug ("## cmdline at 0x%08lX ", top);
	strcpy (cmdline, s);

	/* copy bdinfo */
	top = (top - sizeof (bd_t)) & ~0xF;
	debug ("## bd at 0x%08lX ", top);
	kbd = (bd_t *)top;
	memcpy (kbd, gd->bd, sizeof (bd_t));

	/* first find number of env entries, and their size */
	envno = 0;
	envsz = 0;
	for (i = 0; env_get_char (i) != '\0'; i = nxt + 1) {
		for (nxt = i; env_get_char (nxt) != '\0'; ++nxt)
			;
		envno++;
		envsz += (nxt - i) + 1;	/* plus trailing zero */
	}
	envno++;	/* plus the terminating zero */
	debug ("## %u envvars total size %u ", envno, envsz);

	top = (top - sizeof (char **) * envno) & ~0xF;
	fwenv = (char **)top;
	debug ("## fwenv at 0x%08lX ", top);

	top = (top - envsz) & ~0xF;
	s = (char *)top;
	ss = fwenv;

	/* now copy them */
	for (i = 0; env_get_char (i) != '\0'; i = nxt + 1) {
		for (nxt = i; env_get_char (nxt) != '\0'; ++nxt)
			;
		*ss++ = s;
		for (j = i; j < nxt; ++j)
			*s++ = env_get_char (j);
		*s++ = '\0';
	}
	*ss++ = NULL;	/* terminate */

	entry = (void (*)(bd_t *, char *, char **, ulong))image_get_ep (hdr);
	(*entry) (kbd, cmdline, fwenv, top);
}
#endif
void
print_image_hdr (image_header_t *hdr)
{
#if defined(CONFIG_CMD_DATE) || defined(CONFIG_TIMESTAMP)
	time_t timestamp = (time_t)ntohl(hdr->ih_time);
	struct rtc_time tm;
#endif

	printf ("   Image Name:   %.*s\n", IH_NMLEN, hdr->ih_name);
#if defined(CONFIG_CMD_DATE) || defined(CONFIG_TIMESTAMP)
	to_tm (timestamp, &tm);
	printf ("   Created:      %4d-%02d-%02d  %2d:%02d:%02d UTC\n",
		tm.tm_year, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif	/* CONFIG_CMD_DATE, CONFIG_TIMESTAMP */
	puts ("   Image Type:   "); print_type(hdr);
	printf ("\n   Data Size:    %d Bytes = ", ntohl(hdr->ih_size));
	print_size (ntohl(hdr->ih_size), "\n");
	printf ("   Load Address: %08x\n"
		"   Entry Point:  %08x\n",
		 ntohl(hdr->ih_load), ntohl(hdr->ih_ep));

	if (hdr->ih_type == IH_TYPE_MULTI) {
		int i;
		ulong len;
		ulong *len_ptr = (ulong *)((ulong)hdr + sizeof(image_header_t));

		puts ("   Contents:\n");
		for (i=0; (len = ntohl(*len_ptr)); ++i, ++len_ptr) {
			printf ("   Image %d: %8ld Bytes = ", i, len);
			print_size (len, "\n");
		}
	}
}

static void
print_type (image_header_t *hdr)
{
	char *os, *arch, *type, *comp;

	switch (hdr->ih_os) {
	case IH_OS_INVALID:	os = "Invalid OS";		break;
	case IH_OS_NETBSD:	os = "NetBSD";			break;
	case IH_OS_LINUX:	os = "Linux";			break;
	case IH_OS_VXWORKS:	os = "VxWorks";			break;
	case IH_OS_QNX:		os = "QNX";			break;
	case IH_OS_U_BOOT:	os = "U-Boot";			break;
	case IH_OS_RTEMS:	os = "RTEMS";			break;
#ifdef CONFIG_ARTOS
	case IH_OS_ARTOS:	os = "ARTOS";			break;
#endif
#ifdef CONFIG_LYNXKDI
	case IH_OS_LYNXOS:	os = "LynxOS";			break;
#endif
	default:		os = "Unknown OS";		break;
	}

	switch (hdr->ih_arch) {
	case IH_ARCH_INVALID:	arch = "Invalid CPU";		break;
	case IH_ARCH_ALPHA:	arch = "Alpha";			break;
	case IH_ARCH_ARM:	arch = "ARM";			break;
	case IH_ARCH_AVR32:	arch = "AVR32";			break;
	case IH_ARCH_I386:	arch = "Intel x86";		break;
	case IH_ARCH_IA64:	arch = "IA64";			break;
	case IH_ARCH_MIPS:	arch = "MIPS";			break;
	case IH_ARCH_MIPS64:	arch = "MIPS 64 Bit";		break;
	case IH_ARCH_PPC:	arch = "PowerPC";		break;
	case IH_ARCH_S390:	arch = "IBM S390";		break;
	case IH_ARCH_SH:		arch = "SuperH";		break;
	case IH_ARCH_SPARC:	arch = "SPARC";			break;
	case IH_ARCH_SPARC64:	arch = "SPARC 64 Bit";		break;
	case IH_ARCH_M68K:	arch = "M68K"; 			break;
	case IH_ARCH_MICROBLAZE:	arch = "Microblaze"; 		break;
	case IH_ARCH_NIOS:	arch = "Nios";			break;
	case IH_ARCH_NIOS2:	arch = "Nios-II";		break;
	default:		arch = "Unknown Architecture";	break;
	}

	switch (hdr->ih_type) {
	case IH_TYPE_INVALID:	type = "Invalid Image";		break;
	case IH_TYPE_STANDALONE:type = "Standalone Program";	break;
	case IH_TYPE_KERNEL:	type = "Kernel Image";		break;
	case IH_TYPE_RAMDISK:	type = "RAMDisk Image";		break;
	case IH_TYPE_MULTI:	type = "Multi-File Image";	break;
	case IH_TYPE_FIRMWARE:	type = "Firmware";		break;
	case IH_TYPE_SCRIPT:	type = "Script";		break;
	case IH_TYPE_FLATDT:	type = "Flat Device Tree";	break;
	default:		type = "Unknown Image";		break;
	}

	switch (hdr->ih_comp) {
	case IH_COMP_NONE:	comp = "uncompressed";		break;
	case IH_COMP_GZIP:	comp = "gzip compressed";	break;
	case IH_COMP_BZIP2:	comp = "bzip2 compressed";	break;
	default:		comp = "unknown compression";	break;
	}

	printf ("%s %s %s (%s)", arch, os, type, comp);
}
