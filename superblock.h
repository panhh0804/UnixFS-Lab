/* superblock.h: 超级块管理模块 */
#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

/* 格式化时初始化超级块（含成组链接法组长块链创建） */
void sb_init_format(void);

/* 从磁盘读取超级块到全局 filsys */
void sb_read(void);

/* 将全局 filsys 写回磁盘 */
void sb_write(void);

/* 如果超级块被修改(s_fmod)，则同步写回磁盘 */
void sb_sync(void);

#endif
