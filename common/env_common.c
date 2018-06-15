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
// �����õ���CONFIG_S5PC110
#if defined(CONFIG_S3C6410) || defined(CONFIG_S3C6430) || defined(CONFIG_S5P6440) || defined(CONFIG_S5PC100) || defined(CONFIG_S5PC110) || defined(CONFIG_S5P6442)
// CFG_ENV_SIZE�궨���ǻ��������ķ�����С(16kb)
// default_environment�ַ������о��Ǻö�����ΰ��ŵ��ַ������������һ���ַ���һ��\0��һ���ַ���һ��\0
uchar default_environment[CFG_ENV_SIZE] = {
#else
uchar default_environment[] = {
#endif
#ifdef	CONFIG_BOOTARGS
// gcc�а�˫�����������ĺö���ַ������ΰ����Ž�ȥ�����ΰ�����ʼ����default_environment������
// �������Զ���"bootargs="��CONFIG_BOOTARGS�����ַ��������������м䲻���\0
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
		c = default_environment[index];		// ����default_environmentָ��λ�õ��ַ�
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
	// gd->env_valid��env_movi.c�е�env_init�����ж�����(gd->env_valid = 1;)
	if (gd->env_valid) {
		// gd->env_addr��env_movi.c�е�env_init�����ж�����(gd->env_addr  = (ulong)&default_environment[0];)
		// gd->env_addr���ǻ����������ڴ��е��׵�ַ
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
	if (gd->flags & GD_FLG_RELOC)			// �ж�flag�Ƿ���λ��
		c = env_get_char_memory(index);
	else
		// ���gd->flagsû��GD_FLG_RELOC��־λ����˵��DDR��û�л�������
		// ����env_get_char_init�������þ�����DDR�е���һ�ݻ���������Ч
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
	// ��env_ptr��ָ����Ƕ��ڴ�����
	memset(env_ptr, 0, sizeof(env_t));
	// memcpy������default_environmentĬ�ϻ����������Ƶ��ڴ��env_ptr->data��
	memcpy(env_ptr->data, default_environment,
	       sizeof(default_environment));
#ifdef CFG_REDUNDAND_ENVIRONMENT
	env_ptr->flags = 0xFF;
#endif
	// ����������CRC����У��
	env_crc_update ();
	// ��gd->env_valid��ֵΪ1���´�env_relocate���������ض�λʱSD���о����ˣ�����ִ��uboot��
	// Ĭ�ϵĻ��������ˡ�
	gd->env_valid = 1;
}

void env_relocate (void)
{
	DEBUGF ("%s[%d] offset = 0x%lx\n", __FUNCTION__,__LINE__,
		gd->reloc_off);

#ifdef CONFIG_AMIGAONEG3SE
	enable_nvram();
#endif

#ifdef ENV_IS_EMBEDDED		// �������û�ж����
	/*
	 * The environment buffer is embedded with the text segment�����,
	 * just relocate the environment pointer
	 */
	env_ptr = (env_t *)((ulong)env_ptr + gd->reloc_off);
	DEBUGF ("%s[%d] embedded ENV at %p\n", __FUNCTION__,__LINE__,env_ptr);
#else
	/*
	 * We must allocate a buffer for the environment�����ǵĻ�����������SD����ĳ��������
	 */
	// CFG_ENV_SIZE���������Ĵ�С
	env_ptr = (env_t *)malloc (CFG_ENV_SIZE);		// envָ���ڴ��е�һ��λ��
	DEBUGF ("%s[%d] malloced ENV at %p\n", __FUNCTION__,__LINE__,env_ptr);
#endif
// �����������״�������? 
// SD������һЩ(8��)������������Ϊ���������洢����ġ�����������¼/����ʱ������ֻ����¼��uboot
// ������kernel������rootfs����������������¼env���������Ե�������¼��ϵͳ��һ������ʱENV������
// �յģ���������uboot����ȥSD����ENV������ȡ��������ʱʧ��(��ȡ���������CRCУ��ʱʧ��)������
// ubootѡ���uboot�ڲ����������õ�һ��Ĭ�ϵĻ�������������ʹ��(�����Ĭ�ϻ�������)������Ĭ�ϵ�
// ���������ڱ�������ʱ�ᱻ��ȡ��DDR�еĻ��������У�Ȼ��д��(Ҳ��������saveenvʱд�룬Ҳ������
// uboot����˵�һ�ζ�ȡĬ�ϻ����������д��)SD����ENV������Ȼ���´��ٴο���ʱuboot�ͻ��SD����
// ENV������ȡ����������DDR�У���ζ�ȡ�Ͳ���ʧ���ˡ�
	if (gd->env_valid == 0) {
		// uboot��һ��������ʹ��Ĭ�ϵĻ�������
#if defined(CONFIG_GTH)	|| defined(CFG_ENV_IS_NOWHERE)	/* Environment not changable */
		puts ("Using default environment\n\n");
#else
		puts ("*** Warning - bad CRC, using default environment\n\n");
		// ��ʾ����״̬
		show_boot_progress (-60);
#endif
		set_default_env();
	}
	else {
		// uboot�ڶ���������ʹ�õ���SD���еĻ�������(������������ǽ�Ĭ�ϵĻ���������ȡ��DDR��
		// Ȼ���DDR��д��SD����ENV������)
		env_relocate_spec ();
	}
	// ��󻷾������ض�λ�����ڴ��ܵ�env_ptr->data����Ȼ��env_ptr->data�����ݸ�ֵ��
	// gd->env_addrȫ�ֱ�����env_addr��ַ����
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
