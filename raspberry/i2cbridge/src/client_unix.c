/*
 * Copyright (c) 2014 Martin Rödel aka Yomin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/un.h>

#include "i2cbridge.h"

#define printfv(...) if(verbose) printf(__VA_ARGS__)

void usage(char *name)
{
    printf("Usage: %s [-v] [-u <unixpath>] <cmd> <addr> <reg> [<data>]\n", name);
    printf("cmd: read8, read16, write8, write16\n");
    printf("addr/reg/data: hex\n");
    exit(-1);
}

int main(int argc, char *argv[])
{
    int sock;
    char *path = I2CBRIDGE_PWD "/" I2CBRIDGE_UNIX;
    int verbose = 0;
    
    int opt;
    struct sockaddr_un addr;
    struct i2cbridge_request req;
    struct i2cbridge_response res;
    
    while((opt = getopt(argc, argv, "vu:")) != -1)
    {
        switch(opt)
        {
        case 'u':
            path = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            usage(argv[0]);
        }
    }
    
    if(argc-optind < 3 || argc-optind > 4)
        usage(argv[0]);
    
    memset(&req, 0, sizeof(struct i2cbridge_request));
    
    if(!strcmp("read8", argv[optind]))
        req.cmd = I2CBRIDGE_CMD_READ8;
    else if(!strcmp("read16", argv[optind]))
        req.cmd = I2CBRIDGE_CMD_READ16;
    else if(!strcmp("write8", argv[optind]))
        req.cmd = I2CBRIDGE_CMD_WRITE8;
    else if(!strcmp("write16", argv[optind]))
        req.cmd = I2CBRIDGE_CMD_WRITE16;
    else
        usage(argv[0]);
    
    if(!sscanf(argv[optind+1], "%hhx", &req.addr))
    {
        printf("addr not hex\n");
        return -1;
    }
    if(!sscanf(argv[optind+2], "%hhx", &req.reg))
    {
        printf("reg not hex\n");
        return -1;
    }
    if(optind == argc-4 && !sscanf(argv[optind+3], "%hx", &req.data))
    {
        printf("data not hex\n");
        return -1;
    }
    
    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("Failed to open socket");
        return -2;
    }
    
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, UNIX_PATH_MAX, "%s", path);
    
    if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1)
    {
        perror("Failed to connect");
        return -3;
    }
    
    if(send(sock, &req, sizeof(struct i2cbridge_request), 0) == -1)
    {
        perror("Failed to send");
        return -4;
    }
    
    if(recv(sock, &res, sizeof(struct i2cbridge_response), 0) == -1)
    {
        perror("Failed to recv");
        return -5;
    }
    
    switch(res.status)
    {
    case I2CBRIDGE_ERROR_OK:
        printfv("response: ok\ndata: 0x");
        printf("%04hx\n", res.data);
        break;
    case I2CBRIDGE_ERROR_INTERNAL:
        printfv("response: internal error\n");
        break;
    case I2CBRIDGE_ERROR_COMMAND:
        printfv("response: unknown command\n");
        break;
    case I2CBRIDGE_ERROR_ADDRESS:
        printfv("response: device with address not found\n");
        break;
    case I2CBRIDGE_ERROR_I2C:
        printfv("response: error while accessing i2c bus\n");
        break;
    }
    
    close(sock);
    
    return res.status;
}
