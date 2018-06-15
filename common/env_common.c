/*
 * (C) Copyright 2000-2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_AMIGAONEG3SE
	extern void enable_nvram(void);
	extern void disable_nvram(void);
#endif

#undef DEBUG_ENV
#ifdef DEBUG_ENV
#define DEBUGF(fmt,args...) printf(fmt ,##args)
#else
#define DEBUGF(fmt,args...)
#endif

extern env_t *env_ptr;

extern void env_relocate_spec (void);
extern uchar env_get_char_spec(int);

static uchar env_get_char_init (int index);

/************************************************************************
 * Default settings to be used when no valid environment is found
 */
#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)
// 我们用的是CONFIG_S5PC110
#if defined(CONFIG_S3C6410) || defined(CONFIG_S3C6430) || defined(CONFIG_S5P6440) || defined(CONFIG_S5PC100) || defined(CONFIG_S5PC110) || defined(CONFIG_S5P6442)
// CFG_ENV_SIZE宏定义是环境变量的分区大小(16kb)
// default_environment字符数组中就是好多个依次挨着的字符串，里面就是一个字符串一个\0，一个字符串一个\0
uchar default_environment[CFG_ENV_SIZE] = {
#else
uchar default_environment[] = {
#endif
#ifdef	CONFIG_BOOTARGS
// gcc中把双引号引起来的好多个字符串依次挨个放进去，依次挨个初始化到default_environment数组中
// 编译器自动将"bootargs="和CONFIG_BOOTARGS两个字符串连接起来，中间不会加\0
	"bootargs="	CONFIG_BOOTARGS			"\0"
#endif
#ifdef	CONFIG_BOOTCOMMAND
	"bootcmd="	CONFIG_BOOTCOMMAND		"\0"
#endif
#if 0	/* for fast booting */
	"verify="		MK_STR(no)					"\0"
#endif
#ifdef	CONFIG_MTDPARTITION
	"mtdpart="	CONFIG_MTDPARTITION		"\0"
#endif
#ifdef	CONFIG_RAMBOOTCOMMAND
	"ramboot="	CONFIG_RAMBOOTCOMMAND		"\0"
#endif
#ifdef	CONFIG_NFSBOOTCOMMAND
	"nfsboot="	CONFIG_NFSBOOTCOMMAND		"\0"
#endif
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	"bootdelay="	MK_STR(CONFIG_BOOTDELAY)	"\0"
#endif
#if defined(CONFIG_BAUDRATE) && (CONFIG_BAUDRATE >= 0)
	"baudrate="	MK_STR(CONFIG_BAUDRATE)		"\0"
#endif
#ifdef	CONFIG_LOADS_ECHO
	"loads_echo="	MK_STR(CONFIG_LOADS_ECHO)	"\0"
#endif
#ifdef	CONFIG_ETHADDR
	"ethaddr="	MK_STR(CONFIG_ETHADDR)		"\0"
#endif
#ifdef	CONFIG_ETH1ADDR
	"eth1addr="	MK_STR(CONFIG_ETH1ADDR)		"\0"
#endif
#ifdef	CONFIG_ETH2ADDR
	"eth2addr="	MK_STR(CONFIG_ETH2ADDR)		"\0"
#endif
#ifdef	CONFIG_ETH3ADDR
	"eth3addr="	MK_STR(CONFIG_ETH3ADDR)		"\0"
#endif
#ifdef	CONFIG_IPADDR
	"ipaddr="	MK_STR(CONFIG_IPADDR)		"\0"
#endif
#ifdef	CONFIG_SERVERIP
	"serverip="	MK_STR(CONFIG_SERVERIP)		"\0"
#endif
#ifdef	CFG_AUTOLOAD
	"autoload="	CFG_AUTOLOAD			"\0"
#endif
#ifdef	CONFIG_PREBOOT
	"preboot="	CONFIG_PREBOOT			"\0"
#endif
#ifdef	CONFIG_ROOTPATH
	"rootpath="	MK_STR(CONFIG_ROOTPATH)		"\0"
#endif
#ifdef	CONFIG_GATEWAYIP
	"gatewayip="	MK_STR(CONFIG_GATEWAYIP)	"\0"
#endif
#ifdef	CONFIG_NETMASK
	"netmask="	MK_STR(CONFIG_NETMASK)		"\0"
#endif
#ifdef	CONFIG_HOSTNAME
	"hostname="	MK_STR(CONFIG_HOSTNAME)		"\0"
#endif
#ifdef	CONFIG_BOOTFILE
	"bootfile="	MK_STR(CONFIG_BOOTFILE)		"\0"
#endif
#ifdef	CONFIG_LOADADDR
	"loadaddr="	MK_STR(CONFIG_LOADADDR)		"\0"
#endif
#ifdef  CONFIG_CLOCKS_IN_MHZ
	"clocks_in_mhz=1\0"
#endif
#if defined(CONFIG_PCI_BOOTDELAY) && (CONFIG_PCI_BOOTDELAY > 0)
	"pcidelay="	MK_STR(CONFIG_PCI_BOOTDELAY)	"\0"
#endif
#ifdef  CONFIG_EXTRA_ENV_SETTINGS
	CONFIG_EXTRA_ENV_SETTINGS
#endif
	"\0"
};

#if defined(CFG_ENV_IS_IN_NAND) || defined(CFG_ENV_IS_IN_MOVINAND) || defined(CFG_ENV_IS_IN_ONENAND) || defined(CFG_ENV_IS_IN_AUTO) || defined(CFG_ENV_IS_IN_SPI_FLASH) /* Environment is in Nand Flash or MoviNAND or OneNAND */

int default_environment_size = sizeof(default_environment);
#endif

void env_crc_update (void)
{
	env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE);
}

static uchar env_get_char_init (int index)
{
	uchar c;

	/* if crc was bad, use the default environment */
	if (gd->env_valid)
	{
		c = env_get_char_spec(index);
	} else {
		c = default_environment[index];		// 返回default_environment指定位置的字符
	}

	return (c);
}

#ifdef CONFIG_AMIGAONEG3SE
uchar env_get_char_memory (int index)
{
	uchar retval;
	enable_nvram();
	if (gd->env_valid) {
		retval = ( *((uchar *)(gd->env_addr + index)) );
	} else {
		retval = ( default_environment[index] );
	}
	disable_nvram();
	return retval;
}
#else
uchar env_get_char_memory (int index)
{
	// gd->env_valid在env_movi.c中的env_init函数中定义了(gd->env_valid = 1;)
	if (gd->env_valid) {
		// gd->env_addr在env_movi.c中的env_init函数中定义了(gd->env_addr  = (ulong)&default_environment[0];)
		// gd->env_addr就是环境变量在内存中的首地址
		return ( *((uchar *)(gd->env_addr + index)) );
	} else {
		return ( default_environment[index] );
	}
}
#endif

uchar env_get_char (int index)
{
	uchar c;

	/* if relocated to RAM */
	if (gd->flags & GD_FLG_RELOC)			// 判断flag是否被置位了
		c = env_get_char_memory(index);
	else
		// 如果gd->flags没有GD_FLG_RELOC标志位，则说明DDR中没有环境变量
		// 所以env_get_char_init函数作用就是让DDR中的那一份环境变量有效
		c = env_get_char_init(index);

	return (c);
}

uchar *env_get_addr (int index)
{
	if (gd->env_valid) {
		return ( ((uchar *)(gd->env_addr + index)) );
	} else {
		return (&default_environment[index]);
	}
}

void set_default_env(void)
{
	if (sizeof(default_environment) > ENV_SIZE) {
		puts ("*** Error - default environment is too large\n\n");
		return;
	}
	// 把env_ptr所指向的那段内存清零
	memset(env_ptr, 0, sizeof(env_t));
	// memcpy函数把default_environment默认环境变量复制到内存的env_ptr->data中
	memcpy(env_ptr->data, default_environment,
	       sizeof(default_environment));
#ifdef CFG_REDUNDAND_ENVIRONMENT
	env_ptr->flags = 0xFF;
#endif
	// 环境变量的CRC进行校验
	env_crc_update ();
	// 把gd->env_valid赋值为1，下次env_relocate环境变量重定位时SD卡中就有了，不用执行uboot中
	// 默认的环境变量了。
	gd->env_valid = 1;
}

void env_relocate (void)
{
	DEBUGF ("%s[%d] offset = 0x%lx\n", __FUNCTION__,__LINE__,
		gd->reloc_off);

#ifdef CONFIG_AMIGAONEG3SE
	enable_nvram();
#endif

#ifdef ENV_IS_EMBEDDED		// 这个宏是没有定义的
	/*
	 * The environment buffer is embedded with the text segment代码段,
	 * just relocate the environment pointer
	 */
	env_ptr = (env_t *)((ulong)env_ptr + gd->reloc_off);
	DEBUGF ("%s[%d] embedded ENV at %p\n", __FUNCTION__,__LINE__,env_ptr);
#else
	/*
	 * We must allocate a buffer for the environment，我们的环境变量放在SD卡的某个扇区中
	 */
	// CFG_ENV_SIZE环境变量的大小
	env_ptr = (env_t *)malloc (CFG_ENV_SIZE);		// env指向内存中的一个位置
	DEBUGF ("%s[%d] malloced ENV at %p\n", __FUNCTION__,__LINE__,env_ptr);
#endif
// 环境变量到底从哪里来? 
// SD卡中有一些(8个)独立的扇区作为环境变量存储区域的。但是我们烧录/部署时，我们只是烧录了uboot
// 分区、kernel分区和rootfs分区，根本不曾烧录env分区。所以当我们烧录完系统第一次启动时ENV分区是
// 空的，本次启动uboot尝试去SD卡的ENV分区读取环境变量时失败(读取回来后进行CRC校验时失败)，我们
// uboot选择从uboot内部代码中设置的一套默认的环境变量出发来使用(这就是默认环境变量)；这套默认的
// 环境变量在本次运行时会被读取到DDR中的环境变量中，然后被写入(也可能是你saveenv时写入，也可能是
// uboot设计了第一次读取默认环境变量后就写入)SD卡的ENV分区。然后下次再次开机时uboot就会从SD卡的
// ENV分区读取环境变量到DDR中，这次读取就不会失败了。
	if (gd->env_valid == 0) {
		// uboot第一次启动，使用默认的环境变量
#if defined(CONFIG_GTH)	|| defined(CFG_ENV_IS_NOWHERE)	/* Environment not changable */
		puts ("Using default environment\n\n");
#else
		puts ("*** Warning - bad CRC, using default environment\n\n");
		// 显示启动状态
		show_boot_progress (-60);
#endif
		set_default_env();
	}
	else {
		// uboot第二次启动，使用的是SD卡中的环境变量(这个环境变量是将默认的环境变量读取到DDR中
		// 然后从DDR中写到SD卡的ENV分区中)
		env_relocate_spec ();
	}
	// 最后环境变量重定位到了内存总的env_ptr->data区域，然后将env_ptr->data的数据赋值到
	// gd->env_addr全局变量的env_addr地址处。
	gd->env_addr = (ulong)&(env_ptr->data);

#ifdef CONFIG_AMIGAONEG3SE
	disable_nvram();
#endif
}

#ifdef CONFIG_AUTO_COMPLETE
int env_complete(char *var, int maxv, char *cmdv[], int bufsz, char *buf)
{
	int i, nxt, len, vallen, found;
	const char *lval, *rval;

	found = 0;
	cmdv[0] = NULL;

	len = strlen(var);
	/* now iterate over the variables and select those that match */
	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt)
			;

		lval = (char *)env_get_addr(i);
		rval = strchr(lval, '=');
		if (rval != NULL) {
			vallen = rval - lval;
			rval++;
		} else
			vallen = strlen(lval);

		if (len > 0 && (vallen < len || memcmp(lval, var, len) != 0))
			continue;

		if (found >= maxv - 2 || bufsz < vallen + 1) {
			cmdv[found++] = "...";
			break;
		}
		cmdv[found++] = buf;
		memcpy(buf, lval, vallen); buf += vallen; bufsz -= vallen;
		*buf++ = '\0'; bufsz--;
	}

	cmdv[found] = NULL;
	return found;
}
#endif
