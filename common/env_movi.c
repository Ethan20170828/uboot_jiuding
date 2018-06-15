#include <common.h>

#if defined(CFG_ENV_IS_IN_MOVINAND) /* Environment is in MoviNAND */

#include <environment.h>
#include <movi.h>

#if defined(CONFIG_CMD_ENV) || defined(CONFIG_CMD_NAND) || defined(CONFIG_CMD_MOVINAND) || defined(CONFIG_CMD_ONENAND)
#define CMD_SAVEENV
#endif

/* references to names in env_common.c */
extern uchar default_environment[];
extern int default_environment_size;

char * env_name_spec = "moviNAND";

#ifdef ENV_IS_EMBEDDED
extern uchar environment[];
env_t *env_ptr = (env_t *)(&environment[0]);
#else /* ! ENV_IS_EMBEDDED */
env_t *env_ptr = 0;
#endif /* ENV_IS_EMBEDDED */

/* local functions */
#if !defined(ENV_IS_EMBEDDED)
static void use_default(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

uchar env_get_char_spec (int index)
{
	// gd->env_addr就是环境变量在内存中的首地址
	return ( *((uchar *)(gd->env_addr + index)) );
}

// 经过基本分析，这个函数只是对内存里维护的那一份uboot的环境变量做了基本的初始化或者说是判定(判定
// 里面有没有能用的环境变量)，当前因为我们还没进行环境变量从SD卡到DDR中的relocate，因此当前环境变量
// 是不能用的。只能到SD卡中读取环境变量，不能在DDR中读取环境变量。
int env_init(void)
{
#if defined(ENV_IS_EMBEDDED)
	ulong total;
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1, *tmp_env2;

	total = CFG_ENV_SIZE;

	tmp_env1 = env_ptr;
	tmp_env2 = (env_t *)((ulong)env_ptr + CFG_ENV_SIZE);

	crc1_ok = (crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);
	crc2_ok = (crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);

	if (!crc1_ok && !crc2_ok)
		gd->env_valid = 0;
	else if(crc1_ok && !crc2_ok)
		gd->env_valid = 1;
	else if(!crc1_ok && crc2_ok)
		gd->env_valid = 2;
	else {
		/* both ok - check serial */
		if(tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if(tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if(tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if(tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;
	}

	if (gd->env_valid == 1)
		env_ptr = tmp_env1;
	else if (gd->env_valid == 2)
		env_ptr = tmp_env2;
#else /* ENV_IS_EMBEDDED */
// 把默认的环境变量的数组的首地址放到了gd->env_addr环境变量在内存中的地址
	gd->env_addr  = (ulong)&default_environment[0];
	gd->env_valid = 1;
#endif /* ENV_IS_EMBEDDED */

	return (0);
}

#ifdef CMD_SAVEENV
int saveenv(void)
{
	movi_write_env(virt_to_phys((ulong)env_ptr));
	puts ("done\n");

	return 1;
}
// 函数执行环境变量从SD卡到DDR中的重定位
void env_relocate_spec (void)
{
#if !defined(ENV_IS_EMBEDDED)
	uint *magic = (uint*)(PHYS_SDRAM_1);

	if ((0x24564236 != magic[0]) || (0x20764316 != magic[1]))
		movi_read_env(virt_to_phys((ulong)env_ptr));	// 环境变量从SD卡重定位到DDR中
	// 如果环境变量从SD卡重定位到DDR中后，CRC校验不通过，则继续使用默认的环境变量
	if (crc32(0, env_ptr->data, ENV_SIZE) != env_ptr->crc)
		return use_default();
#endif /* ! ENV_IS_EMBEDDED */
}

#if !defined(ENV_IS_EMBEDDED)
static void use_default()
{
	puts ("*** Warning - bad CRC or moviNAND, using default environment\n\n");

	if (default_environment_size > CFG_ENV_SIZE){
		puts ("*** Error - default environment is too large\n\n");
		return;
	}

	memset (env_ptr, 0, sizeof(env_t));
	memcpy (env_ptr->data,
			default_environment,
			default_environment_size);
	env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE);
	gd->env_valid = 1;
}
#endif

#endif /* CMD_SAVEENV */

#endif /* CFG_ENV_IS_IN_MOVINAND */
