/* wc.c: 统计文件字节数、行数、词数 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "filesys.h"

void file_wc(const char *filename)
{
    uint16_t ffd;
    char buf[BLOCKSIZ];
    int n;
    int in_word = 0;
    uint32_t bytes = 0, words = 0, lines = 0;
    int i;

    ffd = aopen(user_id, filename, FREAD);
    if (ffd >= NOFILE)
    {
        printf(C_BRED "Cannot open '%s'.\n" C_RESET, filename);
        return;
    }

    while ((n = read(ffd, buf, BLOCKSIZ)) > 0)
    {
        bytes += n;
        for (i = 0; i < n; i++)
        {
            if (buf[i] == '\n')
                lines++;
            if (isspace((unsigned char)buf[i]))
                in_word = 0;
            else if (!in_word)
            {
                in_word = 1;
                words++;
            }
        }
    }

    close(user_id, ffd);
    printf(C_BGREEN "%lu" C_RESET " " C_BYELLOW "%lu" C_RESET " " C_BBLUE "%lu" C_RESET " %s\n", (unsigned long)lines, (unsigned long)words, (unsigned long)bytes, filename);
}
