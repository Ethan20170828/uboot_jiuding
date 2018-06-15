/*
 * (C) Copyright 2000
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
 *  Definitions for Command Processor
 */
#ifndef __COMMAND_H
#define __COMMAND_H

#include <config.h>

#ifndef NULL
#define NULL	0
#endif

#ifndef	__ASSEMBLY__
/*
 * Monitor Command Table
 */
// 命令结构体
// uboot的命令体系在工作时，一个命令对应一个cmd_tbl_t结构体的一个实例，然后uboot支持多少个
// 命令，就需要多少个结构体实例。uboot的命令体系把这些结构体实例管理起来，当用户输入了一个
// 命令时，uboot会去这些结构体实例中查找(查找方法和存储管理的方法有关)。如果找到则执行命令，
// 如果未找到则提示命令未知。
// 给命令结构体实例附加特定段属性(用户自定义段)，链接时将带有该段属性的内容链接在一起排列(挨着的，
// 不会夹杂其他东西，也不会丢掉一个带有这种段属性的，但是顺序是乱序的)
// uboot重定位时将该段整体加载到DDR中。加载到DDR中的uboot镜像中带有特定段属性的这一段其实就是命令的
// 集合，有点像一个命令结构体数组。
struct cmd_tbl_s {
	char	*name;		/* Command Name	命令名字		*/
	int		maxargs;	/* maximum number of arguments	命令最多可以接收几个参数*/
	// repeatable等于1表示这个命令可以重复执行，等于0表示这个命令不可以重复执行
	int		repeatable;	/* autorepeat allowed?		命令是否可以接收重复，按下回车可以继续执行上一次命令*/
						/* Implementation function	*/
	// 函数指针，调用这个函数指针来执行相应的命令所对应的函数
	int		(*cmd)(struct cmd_tbl_s *, int, int, char *[]);
	// 打印出命令的简短使用说明
	char		*usage;		/* Usage message	(short)	*/
#ifdef	CFG_LONGHELP
	// 打印出命令的长使用说明
	char		*help;		/* Help  message	(long)	*/
#endif
#ifdef CONFIG_AUTO_COMPLETE
	/* do auto completion on the arguments */
// 函数指针，指向这个命令的自动补全的函数，在uboot中大多数不适用该功能
	int		(*complete)(int argc, char *argv[], char last_char, int maxv, char *cmdv[]);
#endif
};

typedef struct cmd_tbl_s	cmd_tbl_t;

extern cmd_tbl_t  __u_boot_cmd_start;
extern cmd_tbl_t  __u_boot_cmd_end;


/* common/command.c */
cmd_tbl_t *find_cmd(const char *cmd);

#ifdef CONFIG_AUTO_COMPLETE
extern void install_auto_complete(void);
extern int cmd_auto_complete(const char *const prompt, char *buf, int *np, int *colp);
#endif

/*
 * Monitor Command
 *
 * All commands use a common argument format:
 *
 * void function (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
 */

typedef	void	command_t (cmd_tbl_t *, int, int, char *[]);

#endif	/* __ASSEMBLY__ */

/*
 * Command Flags:
 */
#define CMD_FLAG_REPEAT		0x0001	/* repeat last command		*/
#define CMD_FLAG_BOOTD		0x0002	/* command is from bootd	*/

// __attribute__是属性；意思是把它前面的变量贴上一个命令标签(就是所谓的段，就是把前面的命令结构体放到
// .u_boot_cmd的段属性中)
// 而像.text .bss .data段这些都是gcc编译器本身定义的段属性，.u_boot_cmd属于用户的自定义段
#define Struct_Section  __attribute__ ((unused,section (".u_boot_cmd")))

#ifdef  CFG_LONGHELP

// U_BOOT_CMD宏的理解，关键在于结构体变量的名字和段属性。名字使用##作为连字符。
// uboot中不可以接收两个命令的名字一样，U_BOOT_CMD属于全局变量，全局变量名字不能一样，所以
// 才会使用__u_boot_cmd_##name这种方式来区分没有命令。
// 段属性可以理解为把命令贴一个标签，附加了用户自定义段属性，以保证链接时将这些数据结构链接在一起排布
// 通过reference搜索，找到.h文件
// 这个宏其实就是定义一个命令对应的结构体变量，这个变量名和宏的第一个参数有关。
// 因此只要宏调用时传参的第一个参数不同则定义的结构体变量不会重名。
#define U_BOOT_CMD(name,maxargs,rep,cmd,usage,help) \
cmd_tbl_t __u_boot_cmd_##name Struct_Section = {#name, maxargs, rep, cmd, usage, help}
// cmd_tbl_t命令结构体类型，__u_boot_cmd_##name是结构体变量的名字，##在gcc中是扩展语法，是连字符
// #name这个的含义是将name变为字符串类型的，然后赋值给cmd_tbl_t->name。

#else	/* no long help info */

#define U_BOOT_CMD(name,maxargs,rep,cmd,usage,help) \
cmd_tbl_t __u_boot_cmd_##name Struct_Section = {#name, maxargs, rep, cmd, usage}

#endif	/* CFG_LONGHELP */

#endif	/* __COMMAND_H */
