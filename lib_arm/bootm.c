/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * Copyright (C) 2001  Erik Mouw (J.A.K.Mouw@its.tudelft.nl)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 *
 */

#include <common.h>
#include <command.h>
#include <image.h>
#include <zlib.h>
#include <asm/byteorder.h>

DECLARE_GLOBAL_DATA_PTR;

#if defined (CONFIG_SETUP_MEMORY_TAGS) || \
    defined (CONFIG_CMDLINE_TAG) || \
    defined (CONFIG_INITRD_TAG) || \
    defined (CONFIG_SERIAL_TAG) || \
    defined (CONFIG_REVISION_TAG) || \
    defined (CONFIG_VFD) || \
    defined (CONFIG_LCD)
static void setup_start_tag (bd_t *bd);

# ifdef CONFIG_SETUP_MEMORY_TAGS
static void setup_memory_tags (bd_t *bd);
# endif
static void setup_commandline_tag (bd_t *bd, char *commandline);

# ifdef CONFIG_INITRD_TAG
static void setup_initrd_tag (bd_t *bd, ulong initrd_start,
			      ulong initrd_end);
# endif
static void setup_end_tag (bd_t *bd);

# if defined (CONFIG_VFD) || defined (CONFIG_LCD)
static void setup_videolfb_tag (gd_t *gd);
# endif

static struct tag *params;
#endif /* CONFIG_SETUP_MEMORY_TAGS || CONFIG_CMDLINE_TAG || CONFIG_INITRD_TAG */

extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

void do_bootm_linux (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[],
		     bootm_headers_t *images)
{
	ulong	initrd_start, initrd_end;
	ulong	ep = 0;
	bd_t	*bd = gd->bd;
	char	*s;
	// 这里的bd->bi_arch_number值是在board\samsung\x210\x210.c文件中board_init函数中赋值了
	// 把机器码赋值给了局部变量machid
	int	machid = bd->bi_arch_number;
	// 函数指针，传参: 第一个是CPU ID号，第二个是机器码，第三个是内核启动参数
	void	(*theKernel)(int zero, int arch, uint params);
	int	ret;

#ifdef CONFIG_CMDLINE_TAG
	char *commandline = getenv ("bootargs");
#endif

	/* find kernel entry point */
// legacy_hdr_valid变量在之前的do_bootm函数中已经置为1了(images.legacy_hdr_valid = 1;)
// 判断当前镜像是不是一个有意义的uImage/zImage的镜像
// 如果判断成功，那么从ep指向的位置开始执行镜像(这个镜像可能是uImage/zImage两种方式)
	if (images->legacy_hdr_valid) {
// ep就是entrypoint的缩写，就是程序入口。
// 一个镜像文件的起始执行部分不是在镜像的开头(镜像开头有n个字节的头信息)，真正的镜像文件执行时第一句
// 代码在镜像的中部某个字节处，相当于头是有一定的偏移量的。这个偏移量记录在头信息中。
// 一般执行一个镜像都是: 第一步先读取头信息，然后在头信息的特定地址找MAGIC_NUM，由此来确定镜像种类；
// 第二步对镜像进行校验(校验镜像是否完整)；第三步再次读取头信息，由特定地址知道这个镜像的各种信息(
// 镜像长度、镜像种类、入口地址)；第四步去entrypoint处开始执行镜像。
		ep = image_get_ep (&images->legacy_hdr_os_copy);
#if defined(CONFIG_FIT)
	// images->fit_uname_os如果这个置为1，则是设备树方式传参的内核
	} else if (images->fit_uname_os) {
		ret = fit_image_get_entry (images->fit_hdr_os,
					images->fit_noffset_os, &ep);
		if (ret) {
			puts ("Can't get entry point property!\n");
			goto error;
		}
#endif
	} 
	// 如果上面两种方式都不是，则打印出错信息，表示启动失败
	else {
		puts ("Could not find kernel entry point!\n");
		goto error;
	}
// 将ep赋值给theKernel，则这个函数指针就指向了内存中加载的OS镜像的真正入口地址(就是操作系统的第一句执行的代码)
	theKernel = (void (*)(int, int, uint))ep;
	// uboot在启动内核时，机器码要传给内核。
	// 如果环境变量中有machid，则优先使用环境变量中的machid
	// 如果环境变量中没有machid，则使用x210_sd.h中宏定义的机器码(machid)
	s = getenv ("machid");
	if (s) {
		machid = simple_strtoul (s, NULL, 16);
		printf ("Using machid 0x%x from environment\n", machid);
	}

	ret = boot_get_ramdisk (argc, argv, images, IH_ARCH_ARM,
			&initrd_start, &initrd_end);
	if (ret)
		goto error;

	show_boot_progress (15);

	debug ("## Transferring control to Linux (at address %08lx) ...\n",
	       (ulong) theKernel);
// 从128行-162行，就是uboot在给Linux内核准备传递的参数处理
// uboot像kernel传参是以tag方式传参，struct tag，tag是一个数据结构，在uboot和kernel中都有定义tag
// 这个数据结构，而且定义是一样的。可以方便uboot和kernel进行参数之间的联系
// kernel接收到的传参是若干个tag构成的，这些tag由tag_start起始，到tag_end结束。
// 同时uboot放置的传参是若干个tag构成的，这些tag由tag_start起始，到tag_end结束。
// tag传参的方式是由Linux kernel发明的，kernel定义了这种向我传参的方式，uboot只是实现了这种传参方式
// 从而可以支持给kernel传参。

// 条件编译是判断有哪几个传参tag
// CONFIG_SETUP_MEMORY_TAGS，tag_mem，传参内容是内存的配置信息。uboot通过控制CONFIG_SETUP_MEMORY_TAGS宏
// 来决定是否传递这个参数，如果uboot中定义了这个宏，那么uboot编译时这段宏决定的代码就会被编译进uboot中，
// 内存的配置信息就会被传递到kernel中，如果没有定义这个宏，那么这些信息不会被传递到kernel中

// CONFIG_CMDLINE_TAG，tag_cmdline，传参内容是启动命令行(告诉内核以什么方式来启动)参数，也就是uboot环境变量的bootargs。
// 如果uboot中没有定义这个宏，那么uboot不会将bootargs的值传递给kernel，kernel启动的时候可能会错误。

// CONFIG_INITRD_TAG，涉及到Linux kernel启动的基本原理。

// 移植时tag这注意事项:
// 想从uboot传递什么参数到kernel，就把相应的宏定义打开即可，代码不需要动。
// kernel启动不成功，注意传参是否成功。传参不成功首先看uboot中bootargs设置是否正确，其次
// 看uboot是否开启了相应宏以支持传参。
#if defined (CONFIG_SETUP_MEMORY_TAGS) || \
    defined (CONFIG_CMDLINE_TAG) || \
    defined (CONFIG_INITRD_TAG) || \
    defined (CONFIG_SERIAL_TAG) || \
    defined (CONFIG_REVISION_TAG) || \
    defined (CONFIG_LCD) || \
    defined (CONFIG_VFD) || \
    defined (CONFIG_MTDPARTITION)
    // CONFIG_MTDPARTITION，传参内容是iNand/SD卡的分区表。从uboot传递给kernel
    // 用来传递一个起始的tag
    // 起始tag是ATAG_CORE、结束tag是ATAG_NONE，其他的ATAG_XXX都是有效信息tag。
	setup_start_tag (bd);
#ifdef CONFIG_SERIAL_TAG
	setup_serial_tag (&params);
#endif
#ifdef CONFIG_REVISION_TAG
	setup_revision_tag (&params);
#endif
#ifdef CONFIG_SETUP_MEMORY_TAGS
	setup_memory_tags (bd);
#endif
#ifdef CONFIG_CMDLINE_TAG
	setup_commandline_tag (bd, commandline);
#endif
#ifdef CONFIG_INITRD_TAG
	if (initrd_start && initrd_end)
		setup_initrd_tag (bd, initrd_start, initrd_end);
#endif
#if defined (CONFIG_VFD) || defined (CONFIG_LCD)
	setup_videolfb_tag ((gd_t *) gd);
#endif

#ifdef CONFIG_MTDPARTITION
	setup_mtdpartition_tag();
#endif
	// 用来传递结束tag
	setup_end_tag (bd);
#endif

	/* we assume that the kernel is in place */
// 这个是uboot中最后一句打印出来的信息，这句如果能出现，说明uboot整个是成功的，
// 也成功的加载了内核镜像，也校验成功了，也找到了入口地址，也试图去执行了。
// 如果这句后串口就没输出了，说明内核并没有被成功执行。原因一般是:传参(80%)、内核在DDR中的加载地址
	printf ("\nStarting kernel ...\n\n");

#ifdef CONFIG_USB_DEVICE
	{
		extern void udc_disconnect (void);
		udc_disconnect ();
	}
#endif

	cleanup_before_linux ();
// 传递的第三个参数就是tag的首地址，因为params = (struct tag *) bd->bi_boot_params;把tag_start将0x30000100绑定起来了，所以是同一个地址
// 最后uboot运行完，执行内核时，运行时实际把0放入r0，machid放入到了r1中，bd->bi_boot_params放入到了r2中。
// 这个theKernel是uboot执行的最后一句代码，这句执行完了，就是开始执行kernel的第一句代码了

// 思考: 内核如何拿到这些tag?
// uboot最终是调用theKernel函数来执行kernel的，uboot调用这个函数(其实就是kernel)时传递了3个参数。
// 这三个参数就是uboot直接传递给kernel的3个参数，通过寄存器来实现传参的。(第1个参数放在r0中，第2
// 个参数放在r2中，第3个参数放在r3中)第1个参数固定为0，第2个参数是机器码，第3个参数传递的就是大片传参tag的首地址。
	theKernel (0, machid, bd->bi_boot_params);
	/* does not return */
	return;	// 这里的bd->bi_boot_params值是在board\samsung\x210\x210.c文件中board_init函数中赋值了

error:
	do_reset (cmdtp, flag, argc, argv);
	return;
}


#if defined (CONFIG_SETUP_MEMORY_TAGS) || \
    defined (CONFIG_CMDLINE_TAG) || \
    defined (CONFIG_INITRD_TAG) || \
    defined (CONFIG_SERIAL_TAG) || \
    defined (CONFIG_REVISION_TAG) || \
    defined (CONFIG_LCD) || \
    defined (CONFIG_VFD) || \
    defined (CONFIG_MTDPARTITION)
static void setup_start_tag (bd_t *bd)
{
	// params是tag指针的全局变量
	params = (struct tag *) bd->bi_boot_params;			// 30000100，内核传参的起始地址

	params->hdr.tag = ATAG_CORE;						// tag_start起始地址
	params->hdr.size = tag_size (tag_core);

	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;

	params = tag_next (params);
}


#ifdef CONFIG_SETUP_MEMORY_TAGS
static void setup_memory_tags (bd_t *bd)
{
	int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		params->hdr.tag = ATAG_MEM;
		params->hdr.size = tag_size (tag_mem32);

		params->u.mem.start = bd->bi_dram[i].start;
		params->u.mem.size = bd->bi_dram[i].size;
		// tag_next指向下一个tag，说白了就是指针往后移一个tag的大小
		params = tag_next (params);
	}
}
#endif /* CONFIG_SETUP_MEMORY_TAGS */


static void setup_commandline_tag (bd_t *bd, char *commandline)
{
	char *p;

	if (!commandline)
		return;

	/* eat leading white space */
	for (p = commandline; *p == ' '; p++);

	/* skip non-existent command lines so the kernel will still
	 * use its default command line.
	 */
	if (*p == '\0')
		return;

	params->hdr.tag = ATAG_CMDLINE;
	params->hdr.size =
		(sizeof (struct tag_header) + strlen (p) + 1 + 4) >> 2;

	strcpy (params->u.cmdline.cmdline, p);

	params = tag_next (params);
}


#ifdef CONFIG_INITRD_TAG
static void setup_initrd_tag (bd_t *bd, ulong initrd_start, ulong initrd_end)
{
	/* an ATAG_INITRD node tells the kernel where the compressed
	 * ramdisk can be found. ATAG_RDIMG is a better name, actually.
	 */
	params->hdr.tag = ATAG_INITRD2;
	params->hdr.size = tag_size (tag_initrd);

	params->u.initrd.start = initrd_start;
	params->u.initrd.size = initrd_end - initrd_start;

	params = tag_next (params);
}
#endif /* CONFIG_INITRD_TAG */


#if defined (CONFIG_VFD) || defined (CONFIG_LCD)
extern ulong calc_fbsize (void);
static void setup_videolfb_tag (gd_t *gd)
{
	/* An ATAG_VIDEOLFB node tells the kernel where and how large
	 * the framebuffer for video was allocated (among other things).
	 * Note that a _physical_ address is passed !
	 *
	 * We only use it to pass the address and size, the other entries
	 * in the tag_videolfb are not of interest.
	 */
	params->hdr.tag = ATAG_VIDEOLFB;
	params->hdr.size = tag_size (tag_videolfb);

	params->u.videolfb.lfb_base = (u32) gd->fb_base;
	/* Fb size is calculated according to parameters for our panel
	 */
	params->u.videolfb.lfb_size = calc_fbsize();

	params = tag_next (params);
}
#endif /* CONFIG_VFD || CONFIG_LCD */

#ifdef CONFIG_SERIAL_TAG
void setup_serial_tag (struct tag **tmp)
{
	struct tag *params = *tmp;
	struct tag_serialnr serialnr;
	void get_board_serial(struct tag_serialnr *serialnr);

	get_board_serial(&serialnr);
	params->hdr.tag = ATAG_SERIAL;
	params->hdr.size = tag_size (tag_serialnr);
	params->u.serialnr.low = serialnr.low;
	params->u.serialnr.high= serialnr.high;
	params = tag_next (params);
	*tmp = params;
}
#endif

#ifdef CONFIG_REVISION_TAG
void setup_revision_tag(struct tag **in_params)
{
	u32 rev = 0;
	u32 get_board_rev(void);

	rev = get_board_rev();
	params->hdr.tag = ATAG_REVISION;
	params->hdr.size = tag_size (tag_revision);
	params->u.revision.rev = rev;
	params = tag_next (params);
}
#endif  /* CONFIG_REVISION_TAG */

#ifdef CONFIG_MTDPARTITION
void setup_mtdpartition_tag()
{
	char *p, *temp;
	int i = 0;

	p = getenv("mtdpart");

	params->hdr.tag = ATAG_MTDPART;
	params->hdr.size = tag_size (tag_mtdpart);

	for(temp = p; *temp != '\0'; temp++)
	{
		if(*temp == ' ')
		{
			*temp = '\0';
			params->u.mtdpart_info.mtd_part_size[i] = simple_strtoul(p, NULL, 16);

			p = ++temp;
			i++;
		}
	}
	params->u.mtdpart_info.mtd_part_size[i] = simple_strtoul(p, NULL, 16);

	params = tag_next (params);
}
#endif  /* CONFIG_MTDPARTITION */

static void setup_end_tag (bd_t *bd)
{
	params->hdr.tag = ATAG_NONE;		// 结束地址tag_end
	params->hdr.size = 0;
}

#endif /* CONFIG_SETUP_MEMORY_TAGS || CONFIG_CMDLINE_TAG || CONFIG_INITRD_TAG */
