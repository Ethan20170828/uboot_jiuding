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

/**************************************************************************
 *
 * Support for persistent environment data
 *
 * The "environment" is stored as a list of '\0' terminated
 * "name=value" strings. The end of the list is marked by a double
 * '\0'. New entries are always added at the end. Deleting an entry
 * shifts the remaining entries to the front. Replacing an entry is a
 * combination of deleting the old value and adding the new one.
 *
 * The environment is preceeded by a 32 bit CRC over the data part.
 *
 **************************************************************************
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <watchdog.h>
#include <serial.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#if defined(CONFIG_CMD_NET)
#include <net.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#if !defined(CFG_ENV_IS_IN_NVRAM)	&& \
    !defined(CFG_ENV_IS_IN_EEPROM)	&& \
    !defined(CFG_ENV_IS_IN_FLASH)	&& \
    !defined(CFG_ENV_IS_IN_DATAFLASH)	&& \
    !defined(CFG_ENV_IS_IN_NAND)	&& \
    !defined(CFG_ENV_IS_IN_MOVINAND)	&& \
    !defined(CFG_ENV_IS_IN_ONENAND)	&& \
    !defined(CFG_ENV_IS_IN_AUTO)	&& \
	!defined(CFG_ENV_IS_IN_SPI_FLASH)	&& \
    !defined(CFG_ENV_IS_NOWHERE)
# error Define one of CFG_ENV_IS_IN_{NVRAM|EEPROM|FLASH|DATAFLASH|NAND|MOVINAND|ONENAND|NOWHERE|AUTO}
#endif

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/************************************************************************
************************************************************************/

/*
 * Table with supported baudrates (defined in config_xyz.h)
 */
static const unsigned long baudrate_table[] = CFG_BAUDRATE_TABLE;
#define	N_BAUDRATES (sizeof(baudrate_table) / sizeof(baudrate_table[0]))


/************************************************************************
 * Command interface: print one or all environment variables 打印所有和打印一个环境变量
 */
// 这个函数要看懂，首先要明白整个环境变量在内存中是怎么存储的。
// 譬如: 字符串为abc="1"；ad="2"；则在内存的存储方式为: a|b|c|=|1|'\0'|a|d|=|2|'\0|....
// 任意两个环境变量是用\0来分割的
int do_printenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, j, k, nxt;
	int rcode = 0;
	// argc == 1时用双重for循环来依次处理所有的环境变量的打印。
	if (argc == 1) {		/* Print all env variables	*/
		// env_get_char(i) != '\0';主要是循环default_environment数组
		// 第一个for循环就是处理环境变量有多少行(说白了就是有多个环境变量)，可以理解为是竖坐标
		// env_get_char作用是从环境变量中(default_environment数组)找第i个元素(也就是第i个环境变量)是否等于'\0'
		// 局部变量i表示的是有多少个环境变量，也已理解为竖坐标
		// 局部变量nxt表示的是该环境变量有多少个字符，也可以理解为横坐标
		// 局部变量k表示的是从该环境变量的第一个字符开始走，走一个字符，输出一个字符，直到走完该环境变量的所有字符
		for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
			// 里面的for循环就是处理一个环境变量有多少个字符
			// nxt=i第一次循环nxt等于0
			// 如果第i个环境变量中有字符，则env_get_char(nxt) != '\0'肯定成立，那么就从这个
			// 环境变量的第一个字符开始一直往后走，直到走到该环境变量的字符结束遇到'\0'为止，
			// 说明第i个环境变量中的字符已全部走完，说明这次循环结束了，该到下个环境变量了
			for (nxt=i; env_get_char(nxt) != '\0'; ++nxt)
			// 表示for的循环体是空的，意思是循环体不做任何事情，只让nxt往后走，一直走到nxt取到\0
				;
			// 这个for循环，循环的是i和nxt之间的字符
			// 这个for循环是把第i个环境变量中的字符挨个的输出出来。
			for (k=i; k<nxt; ++k)
				// putc是像控制台输出，通过env_get_char函数输出每个环境变量
				putc(env_get_char(k));
			// 这个表示换行
			putc  ('\n');
			// 按ctrlc打断环境变量的输出
			if (ctrlc()) {
				puts ("\n ** Abort\n");
				return 1;
			}
		}

		printf("\nEnvironment size: %d/%ld bytes\n",
			i, (ulong)ENV_SIZE);

		return 0;
	}
	// argc不等于1，则后面的参数就是要打印的环境变量，给哪个就打印哪个
	for (i=1; i<argc; ++i) {	/* print single env variables	*/
		char *name = argv[i];

		k = -1;

		for (j=0; env_get_char(j) != '\0'; j=nxt+1) {

			for (nxt=j; env_get_char(nxt) != '\0'; ++nxt)
				;
			k = envmatch((uchar *)name, j);		// 得到一个字符串，然后与其他的循环匹配
			if (k < 0) {
				continue;
			}
			puts (name);
			putc ('=');
			while (k < nxt)
				putc(env_get_char(k++));
			putc ('\n');
			break;
		}
		if (k < 0) {
			printf ("## Error: \"%s\" not defined\n", name);
			rcode ++;
		}
	}
	return rcode;
}

/************************************************************************
 * Set a new environment variable,
 * or replace or delete an existing one.
 *
 * This function will ONLY work with a in-RAM copy of the environment
 */
// setenv的思路就是: 
// 第一步先去DDR中的环境变量处寻找原来有没有这个变量，如果原来就有则需要覆盖原来的环境变量
// 如果原来没有则在最后新增一个环境变量即可。
// 第一步: 遍历DDR中环境变量的数组，找到原来就有的那个环境变量对应的地址。
// 第二步: 擦除原来的环境变量。
// 第三步: 写入新的环境变量。
// 本来setenv做完上面的就完了，但是还要考虑一些附加的问题。
// 问题一: 环境变量太多超出DDR中的字符数组，溢出的解决方法。
// 问题二: 有些环境变量如baudrate、ipaddr等，在gd中有对应的全局变量。这种环境变量在set更新的时候
// 要同时去更新对应的全局变量，否则就会出现本次运行中环境变量和全局变量不一致的情况。
int _do_setenv (int flag, int argc, char *argv[])
{
	int   i, len, oldval;
	int   console = -1;
	uchar *env, *nxt = NULL;
	char *name;
	bd_t *bd = gd->bd;

	// env_get_addr函数意思是获取数组中首元素的首地址，为了找到要更改的环境变量
	uchar *env_data = env_get_addr(0);

// env_data = NULL，说明根本没有找到想要的字符串(数组中的元素)，也可以这么理解，内存中还没有环境变量数组
	if (!env_data)	/* need copy in RAM */
		return 1;	

	name = argv[1];

	if (strchr(name, '=')) {	// 比较name中是否有等于号
		printf ("## Error: illegal character '=' in variable name \"%s\"\n", name);
		return 1;
	}

	/*
	 * search if variable with this name already exists
	 */
	oldval = -1;
	// *env可以写成*env != '\0'
	// env=env_data意识是指向整个环境变量的开始，也可以理解为指向第一个环境变量
	// 第一步: 遍历DDR中环境变量的数组，找到原来就有的那个环境变量对应的地址
	for (env=env_data; *env; env=nxt+1) {
		for (nxt=env; *nxt; ++nxt)
			;	// 这个for是把数组中的第一个元素走完
		// env-env_data是default_environment环境变量的名字
		// name局部变量是你要修改的环境变量名字(也就是传参传进来的name)
		// envmatch函数就是将你要修改的name与环境变量中的名字(这个名字是等号前的部分)对比
		if ((oldval = envmatch((uchar *)name, env-env_data)) >= 0)
			// 如果对比后>= 0，则跳出，证明找到了我们要修改的那个环境变量了
			break;
	}	// 这时候的env和nxt记录了我们找到的那个位置

	/*
	 * Delete any existing definition 找到后先把原来的都删除掉
	 */
	if (oldval >= 0) {
#ifndef CONFIG_ENV_OVERWRITE

		/*
		 * Ethernet Address and serial# can be set only once,
		 * ver is readonly.
		 */
		if (
#ifdef CONFIG_HAS_UID
		/* Allow serial# forced overwrite with 0xdeaf4add flag */
		    ((strcmp (name, "serial#") == 0) && (flag != 0xdeaf4add)) ||
#else
		    (strcmp (name, "serial#") == 0) ||
#endif
		    ((strcmp (name, "ethaddr") == 0)
#if defined(CONFIG_OVERWRITE_ETHADDR_ONCE) && defined(CONFIG_ETHADDR)
		     && (strcmp ((char *)env_get_addr(oldval),MK_STR(CONFIG_ETHADDR)) != 0)
#endif	/* CONFIG_OVERWRITE_ETHADDR_ONCE && CONFIG_ETHADDR */
		    ) ) {
			printf ("Can't overwrite \"%s\"\n", name);
			return 1;
		}
#endif

		/* Check for console redirection */
		if (strcmp(name,"stdin") == 0) {
			console = stdin;
		} else if (strcmp(name,"stdout") == 0) {
			console = stdout;
		} else if (strcmp(name,"stderr") == 0) {
			console = stderr;
		}

		if (console != -1) {
			if (argc < 3) {		/* Cannot delete it! */
				printf("Can't delete \"%s\"\n", name);
				return 1;
			}

			/* Try assigning specified device */
			if (console_assign (console, argv[2]) < 0)
				return 1;

#ifdef CONFIG_SERIAL_MULTI
			if (serial_assign (argv[2]) < 0)
				return 1;
#endif
		}

		/*
		 * Switch to new baudrate if new baudrate is supported
		 */
		if (strcmp(argv[1],"baudrate") == 0) {
			int baudrate = simple_strtoul(argv[2], NULL, 10);
			int i;
			for (i=0; i<N_BAUDRATES; ++i) {
				if (baudrate == baudrate_table[i])
					break;
			}
			if (i == N_BAUDRATES) {
				printf ("## Baudrate %d bps not supported\n",
					baudrate);
				return 1;
			}
			printf ("## Switch baudrate to %d bps and press ENTER ...\n",
				baudrate);
			udelay(50000);
			// 不止更新默认的环境变量，还要更新全局变量中的环境变量
			// 如果二者都不更新，会出现不一致现象。
			gd->baudrate = baudrate;
#if defined(CONFIG_PPC) || defined(CONFIG_MCF52x2)
			gd->bd->bi_baudrate = baudrate;
#endif

			serial_setbrg ();
			udelay(50000);
			for (;;) {
				if (getc() == '\r')
				      break;
			}
		}
		// 第二步: 擦除掉原来的环境变量
		// 删除匹配到的环境变量的赋值
		// 如果该环境变量的最后一位是'\0'
		if (*++nxt == '\0') {
			// 如果要删除的环境变量在env_data指向的环境变量后面
			if (env > env_data) {
				// 环境变量中的字符往回走
				env--;
			} else {
				*env = '\0';
			}
		} else {
		// 第三步: 写入新的环境变量
			for (;;) {
				*env = *nxt++;		// 把环境变量新的值复制进env中
				if ((*env == '\0') && (*nxt == '\0'))
					break;			// 这个就是复制完了之后的出口
				++env;
			}
		}
		*++env = '\0';			// 这个就是复制完了所有有效内容之后给个'\0'
	}

#ifdef CONFIG_NET_MULTI
	if (strncmp(name, "eth", 3) == 0) {
		char *end;
		int   num = simple_strtoul(name+3, &end, 10);

		if (strcmp(end, "addr") == 0) {
			eth_set_enetaddr(num, argv[2]);
		}
	}
#endif


	/* Delete only ? */
	if ((argc < 3) || argv[2] == NULL) {
		env_crc_update ();
		return 0;
	}

	/*
	 * Append new definition at the end
	 */
	for (env=env_data; *env || *(env+1); ++env)
		;
	if (env > env_data)
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > ENV_SIZE - (env-env_data)
	 */
	len = strlen(name) + 2;
	/* add '=' for first arg, ' ' for all others */
	for (i=2; i<argc; ++i) {
		len += strlen(argv[i]) + 1;
	}
	if (len > (&env_data[ENV_SIZE]-env)) {
		printf ("## Error: environment overflow, \"%s\" deleted\n", name);
		return 1;
	}
	while ((*env = *name++) != '\0')
		env++;
	for (i=2; i<argc; ++i) {
		char *val = argv[i];

		*env = (i==2) ? '=' : ' ';
		while ((*++env = *val++) != '\0')
			;
	}

	/* end is marked with double '\0' */
	*++env = '\0';

	/* Update CRC */
	env_crc_update ();

	/*
	 * Some variables should be updated when the corresponding
	 * entry in the enviornment is changed
	 */

	if (strcmp(argv[1],"ethaddr") == 0) {
		char *s = argv[2];	/* always use only one arg */
		char *e;
		for (i=0; i<6; ++i) {
			bd->bi_enetaddr[i] = s ? simple_strtoul(s, &e, 16) : 0;
			if (s) s = (*e) ? e+1 : e;
		}
#ifdef CONFIG_NET_MULTI
		eth_set_enetaddr(0, argv[2]);
#endif
		return 0;
	}

	if (strcmp(argv[1],"ipaddr") == 0) {
		char *s = argv[2];	/* always use only one arg */
		char *e;
		unsigned long addr;
		bd->bi_ip_addr = 0;
		for (addr=0, i=0; i<4; ++i) {
			ulong val = s ? simple_strtoul(s, &e, 10) : 0;
			addr <<= 8;
			addr  |= (val & 0xFF);
			if (s) s = (*e) ? e+1 : e;
		}
		bd->bi_ip_addr = htonl(addr);
		return 0;
	}
	if (strcmp(argv[1],"loadaddr") == 0) {
		load_addr = simple_strtoul(argv[2], NULL, 16);
		return 0;
	}
#if defined(CONFIG_CMD_NET)
	if (strcmp(argv[1],"bootfile") == 0) {
		copy_filename (BootFile, argv[2], sizeof(BootFile));
		return 0;
	}
#endif

#ifdef CONFIG_AMIGAONEG3SE
	if (strcmp(argv[1], "vga_fg_color") == 0 ||
	    strcmp(argv[1], "vga_bg_color") == 0 ) {
		extern void video_set_color(unsigned char attr);
		extern unsigned char video_get_attr(void);

		video_set_color(video_get_attr());
		return 0;
	}
#endif	/* CONFIG_AMIGAONEG3SE */

	return 0;
}

int setenv (char *varname, char *varvalue)
{
	char *argv[4] = { "setenv", varname, varvalue, NULL };
	if (varvalue == NULL)
		return _do_setenv (0, 2, argv);
	else
		return _do_setenv (0, 3, argv);
}

#ifdef CONFIG_HAS_UID
void forceenv (char *varname, char *varvalue)
{
	char *argv[4] = { "forceenv", varname, varvalue, NULL };
	_do_setenv (0xdeaf4add, 3, argv);
}
#endif

int do_setenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	return _do_setenv (flag, argc, argv);
}

/************************************************************************
 * Prompt for environment variable
 */

#if defined(CONFIG_CMD_ASKENV)
int do_askenv ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	extern char console_buffer[CFG_CBSIZE];
	char message[CFG_CBSIZE];
	int size = CFG_CBSIZE - 1;
	int len;
	char *local_args[4];

	local_args[0] = argv[0];
	local_args[1] = argv[1];
	local_args[2] = NULL;
	local_args[3] = NULL;

	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	/* Check the syntax */
	switch (argc) {
	case 1:
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;

	case 2:		/* askenv envname */
		sprintf (message, "Please enter '%s':", argv[1]);
		break;

	case 3:		/* askenv envname size */
		sprintf (message, "Please enter '%s':", argv[1]);
		size = simple_strtoul (argv[2], NULL, 10);
		break;

	default:	/* askenv envname message1 ... messagen size */
	    {
		int i;
		int pos = 0;

		for (i = 2; i < argc - 1; i++) {
			if (pos) {
				message[pos++] = ' ';
			}
			strcpy (message+pos, argv[i]);
			pos += strlen(argv[i]);
		}
		message[pos] = '\0';
		size = simple_strtoul (argv[argc - 1], NULL, 10);
	    }
		break;
	}

	if (size >= CFG_CBSIZE)
		size = CFG_CBSIZE - 1;

	if (size <= 0)
		return 1;

	/* prompt for input */
	len = readline (message);

	if (size < len)
		console_buffer[size] = '\0';

	len = 2;
	if (console_buffer[0] != '\0') {
		local_args[2] = console_buffer;
		len = 3;
	}

	/* Continue calling setenv code */
	return _do_setenv (flag, len, local_args);
}
#endif

/************************************************************************
 * Look up variable from environment,
 * return address of storage for that variable,
 * or NULL if not found
 */
// getenv中返回的地址只能读不能随便乱写
// getenv函数是不可重入的
char *getenv (char *name)		// name是要获取的环境变量名字
{
	int i, nxt;

	WATCHDOG_RESET();
// 实现方法就是去遍历default_environment数组，挨个拿出所有的环境变量比对name，
// 找到相等的直接返回这个环境变量的首地址(这个首地址是环境变量在DDR中环境变量处的地址)即可。
	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
		int val;

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (NULL);
			}
		}
		if ((val=envmatch((uchar *)name, i)) < 0)		// envmatch函数的返回值是i2(这里就是第二个传参i)，也就是把i2的值赋值给了val
			continue;	// continue意思是没有找到，继续最外层的for循环
		return ((char *)env_get_addr(val));		// env_get_addr函数返回的是我们得到那个环境变量的首地址
	}

	return (NULL);
}
// getenv_r中返回的环境变量是在自己提供的buf中，是可以随便改写加工的
// getenv_r函数是可重入版本。(可重入函数)，为了安全性考虑的
// 可重入意思就是这一部分，你用完了，其他人同样是可以用的，属于公共的部分
// 不可重入意识是这段自己用完了，其他人是不能继续用的，一次性的。
// 以后最好用getenv_r函数，更加安全
int getenv_r (char *name, char *buf, unsigned len)
{
	int i, nxt;
// getenv函数是直接返回这个找到的环境变量在DDR中环境变量处的地址，
// 而getenv_r函数的做法是找到了DDR中环境变量地址后，将这个环境变量复制
// 一份到提供的buffer中，而不动原来DDR中环境变量。
// 差别就是: getenv中返回的地址只能读不能随便乱写，而getenv_r中返回的环境变量
// 是在自己提供的buffer中，是可以随便改写加工的。
	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
		int val, n;

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (-1);
			}
		}
		if ((val=envmatch((uchar *)name, i)) < 0)
			continue;
		/* found; copy out */
		n = 0;
		// 将这个环境变量复制一份到提供的buf中，而不动原来DDR中环境变量
		while ((len > n++) && (*buf++ = env_get_char(val++)) != '\0')
			;
		if (len == n)
			*buf = '\0';
		return (n);
	}
	return (-1);
}

#if ((defined(CFG_ENV_IS_IN_NVRAM) || defined(CFG_ENV_IS_IN_EEPROM) \
    || (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_FLASH)) \
    || (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_NAND)) \
    || (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_ONENAND)) \
	|| (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_MOVINAND)) \
    && !defined(CFG_ENV_IS_NOWHERE)))
int do_saveenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	extern char * env_name_spec;

	printf ("Saving Environment to %s...\n", env_name_spec);

	return (saveenv() ? 1 : 0);
}

#endif


/************************************************************************
 * Match a name / name=value pair
 *
 * s1 is either a simple 'name', or a 'name=value' pair.
 * i2 is the environment index for a 'name2=value2' pair.
 * If the names match, return the index for the value2, else NULL.
 */
// 譬如:比较"ipaddr"和"ipaddr=192.168.1.230"。
int envmatch (uchar *s1, int i2)
{
	// while就是从default_environment字符串的指定位置开始比较，指定位置由env_get_char返回值所定，
	// 比较不一致时函数返回-1.当比较到default_environment字符串的ipaddr时，开始有相同的字符出现了，
	// 这时while值为1，
	while (*s1 == env_get_char(i2++))
		if (*s1++ == '=')
		// 譬如:比较"ipaddr"和"ipaddr=192.168.1.230"。
		// 当比较完"r"后，指针指向了下一个字符即"="，"="之后的字符串正是我们需要的，
		// 所以return(i2);i2是赋值给val的
			return(i2);	
	// 如果传进来的name为空，则是匹配的是倒数第一个环境变量
	if (*s1 == '\0' && env_get_char(i2-1) == '=')
		return(i2);
	return(-1);
}


/**************************************************/

U_BOOT_CMD(
	printenv, CFG_MAXARGS, 1,	do_printenv,
	"printenv- print environment variables\n",
	"\n    - print values of all environment variables\n"
	"printenv name ...\n"
	"    - print value of environment variable 'name'\n"
);

U_BOOT_CMD(
	setenv, CFG_MAXARGS, 0,	do_setenv,
	"setenv  - set environment variables\n",
	"name value ...\n"
	"    - set environment variable 'name' to 'value ...'\n"
	"setenv name\n"
	"    - delete environment variable 'name'\n"
);

#if ((defined(CFG_ENV_IS_IN_NVRAM) || defined(CFG_ENV_IS_IN_EEPROM) \
    || (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_FLASH)) \
    || (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_NAND)) \
    || (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_ONENAND)) \
	|| (defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_MOVINAND)) \
    && !defined(CFG_ENV_IS_NOWHERE)))

U_BOOT_CMD(
	saveenv, 1, 0,	do_saveenv,
	"saveenv - save environment variables to persistent storage\n",
	NULL
);

#endif

#if defined(CONFIG_CMD_ASKENV)

U_BOOT_CMD(
	askenv,	CFG_MAXARGS,	1,	do_askenv,
	"askenv  - get environment variables from stdin\n",
	"name [message] [size]\n"
	"    - get environment variable 'name' from stdin (max 'size' chars)\n"
	"askenv name\n"
	"    - get environment variable 'name' from stdin\n"
	"askenv name size\n"
	"    - get environment variable 'name' from stdin (max 'size' chars)\n"
	"askenv name [message] size\n"
	"    - display 'message' string and get environment variable 'name'"
	"from stdin (max 'size' chars)\n"
);
#endif

#if defined(CONFIG_CMD_RUN)
int do_run (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
U_BOOT_CMD(
	run,	CFG_MAXARGS,	1,	do_run,
	"run     - run commands in an environment variable\n",
	"var [...]\n"
	"    - run the commands in the environment variable(s) 'var'\n"
);
#endif
