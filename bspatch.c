/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2021 Jahyeon Beak
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if 0
__FBSDID("$FreeBSD: src/usr.bin/bsdiff/bspatch/bspatch.c,v 1.1 2005/08/06 01:59:06 cperciva Exp $");
#endif

#include <bzlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>
#include "bspatch.h"

static off_t offtin(u_char *buf)        // 二进制串变数字，并且考虑符号位
{
    off_t y;

    y=buf[7]&0x7F;
    y=y*256;y+=buf[6];
    y=y*256;y+=buf[5];
    y=y*256;y+=buf[4];
    y=y*256;y+=buf[3];
    y=y*256;y+=buf[2];
    y=y*256;y+=buf[1];
    y=y*256;y+=buf[0];

    if(buf[7]&0x80) y=-y;

    return y;
}

// 传入的3个buf是 oldfile newfile patchfile 的内容
int bspatch(u_char *patchBuf, unsigned int patchLen, ssize_t patchOffset, u_char *oldBuf, unsigned int oldLen, u_char *newBuf, unsigned int newLen)
{
    BZFILE * cpfbz2, * dpfbz2, * epfbz2;
    int cbz2err, dbz2err, ebz2err;
    unsigned int oldsize,newsize;
    unsigned int bzctrllen,bzdatalen;
    u_char header[32],buf[8];
    unsigned int oldpos,newpos;
    unsigned int ctrl[3];
    off_t lenread;
    unsigned int i;

    if(!patchBuf || !oldBuf || !newBuf || !patchLen || !oldLen || !newLen)
    {
        printf("param error\n");
        return -1;
    }
    oldsize = oldLen;
    /*
    File format:
        0    8    "BSDIFF40"
        8    8    X
        16    8    Y
        24    8    sizeof(newfile)
        32    X    bzip2(control block)
        32+X    Y    bzip2(diff block)
        32+X+Y    ???    bzip2(extra block)
    with control block a set of triples (x,y,z) meaning "add x bytes
    from oldfile to x bytes from the diff block; copy y bytes from the
    extra block; seek forwards in oldfile by z bytes".
    */

    /* Read header */
    if (patchLen <= 32) {
        err(1, "Corrupt patch, patchLen = %d\n", patchLen);
    }
    memcpy(header, patchBuf + patchOffset, 32);

    /* Check for appropriate magic */
    if (memcmp(header, "BSDIFF40", 8) != 0) {
        printf("Corrupt patch, patch head != BSDIFF40\n");
        return -1;
    }

    /* Read lengths from header */
    bzctrllen=offtin(header+8);                                                    // bzctrllen 读的是X，control block长度
    bzdatalen=offtin(header+16);                                                // bzdatalen 读的是Y，diff block长度
    newsize=offtin(header+24);                                                    // newsize 读的是 newfile 的长度
    if((bzctrllen<0) || (bzdatalen<0) || (newsize<0)) {
        printf("Corrupt patch\n");
        return -1;
    }
    if(newsize > newLen) { // 要构造的长度大于传进来的长度
        printf("newsize > newLen\n");
        return -1;
    }
    if(patchLen < 32 + bzctrllen + bzdatalen) {
        printf("patchLen < 32 + bzctrllen + bzdatalen\n");
        return -1;
    }

    /* Close patch file and re-open it via libbzip2 at the right places */
    // 初始化
    if ((cpfbz2 = BZ2_bzReadOpen_mem(&cbz2err, NULL, 0, 0, NULL, 0, patchBuf+patchOffset+32, bzctrllen)) == NULL) { // 创建 cpfbz2，对应 control block
        printf( "BZ2_bzReadOpen, bz2err = %d", cbz2err);
        return -1;
    }
    if ((dpfbz2 = BZ2_bzReadOpen_mem(&dbz2err, NULL, 0, 0, NULL, 0, patchBuf+patchOffset+32+bzctrllen, bzdatalen)) == NULL) { // 创建 dpfbz2，对应 diff block
        printf( "BZ2_bzReadOpen, bz2err = %d", dbz2err);
        return -1;
    }
    if ((epfbz2 = BZ2_bzReadOpen_mem(&ebz2err, NULL, 0, 0, NULL, 0, patchBuf+patchOffset+32+bzctrllen+bzdatalen, patchLen-patchOffset-32-bzctrllen-bzdatalen)) == NULL){ // 创建 epfbz2，对应 extra block
        printf( "BZ2_bzReadOpen, bz2err = %d", ebz2err);
        return -1;
    }

    oldpos=0;newpos=0;
    while(newpos<newsize) {
        /* Read control data */
        for(i=0;i<=2;i++) {
            lenread = BZ2_bzRead_mem(&cbz2err, cpfbz2, buf, 8); // 从 control block 里面解压出3个8字节，放在 buf 里面，最终变成长度，放在 ctrl[i] 里面
            if ((lenread < 8) || ((cbz2err != BZ_OK) &&
                (cbz2err != BZ_STREAM_END))) {
                printf( "Corrupt patch\n");
                return -1;
            }
            ctrl[i]=offtin(buf);
        };

        /* Sanity-check */
        if(newpos+ctrl[0]>newsize) { //newpos：从哪个位置开始写，ctrl[0]：要写多长
            printf("Corrupt patch\n");
            return -1;
        }

        /* Read diff string */
        lenread = BZ2_bzRead_mem(&dbz2err, dpfbz2, newBuf + newpos, ctrl[0]);    // 从 diff block 里面解压出 ctrl[0] 个字节，放在 newBuf + newpos 里面
        if (((unsigned int)lenread < ctrl[0]) ||
            ((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END))) {
            printf( "Corrupt patch\n");
            return -1;
        }

        /* Add old data to diff string */
        for(i=0;i<ctrl[0];i++) // 用 oldBuf 来修改 newBuf
            if((oldpos+i>=0) && (oldpos+i<oldsize))
                newBuf[newpos+i]+=oldBuf[oldpos+i];

        /* Adjust pointers */
        newpos+=ctrl[0];
        oldpos+=ctrl[0];

        /* Sanity-check */
        if(newpos+ctrl[1]>newsize) { // ctrl[1]是接下来要直接覆盖的长度
            printf("Corrupt patch\n");
            return -1;
        }

        /* Read extra string */
        lenread = BZ2_bzRead_mem(&ebz2err, epfbz2, newBuf + newpos, ctrl[1]);    // 直接从 extra block 解压出ctrl[1]个字节，但是不应该是从 oldBuf 拷贝出来会更好吗？
        if (((unsigned int)lenread < ctrl[1]) ||
            ((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END))) {
            printf( "Corrupt patch\n");
            return -1;
        }

        /* Adjust pointers */
        newpos+=ctrl[1];
        oldpos+=ctrl[2];                                                        // 下一段要用到的 oldBuf 的位置
    };

    /* Clean up the bzip2 reads */
    BZ2_bzReadClose(&cbz2err, cpfbz2);
    BZ2_bzReadClose(&dbz2err, dpfbz2);
    BZ2_bzReadClose(&ebz2err, epfbz2);

    return newpos;
}


