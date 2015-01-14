#define LUA_LIB
#include "lua5.2/lua.h"
#include "lua5.2/lauxlib.h"
#include "lua5.2/lualib.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <gf_rand.h>
#include "jerasure.h"
#include "reed_sol.h"

/*
TODO
- modify the way functions are registered (luaopen_luajerasure) to allow in Lua
  local jerasure = require "luajerasure"
  jerasure.encode()
*/

static void print_data_and_coding(int k, int m, int w, int size, char **data, char **coding) 
{
    int i, j, x;
    int n, sp;

    if(k > m) n = k;
    else n = m;
    sp = size * 2 + size/(w/8) + 8;

    printf("%-*sCoding\n", sp, "Data");
    for(i = 0; i < n; i++) {
        if(i < k) {
            printf("D%-2d:", i);
            for(j=0;j< size; j+=(w/8)) { 
                printf(" ");
                for(x=0;x < w/8;x++){
                    printf("%02x", (unsigned char)data[i][j+x]);
                }
            }
            printf("    ");
        }
        else printf("%*s", sp, "");
        if(i < m) {
            printf("C%-2d:", i);
            for(j=0;j< size; j+=(w/8)) { 
            printf(" ");
            for(x=0;x < w/8;x++){
                printf("%02x", (unsigned char)coding[i][j+x]);
            }
            }
        }
        printf("\n");
    }
    printf("\n");
}

static void check_args(int k, int m, int w) {
    if ( k <= 0 ) {
        printf("Invalid value for k.\n");
        exit(0);
    }
    if ( m <= 0 ) {
        printf("Invalid value for m.\n");
        exit(0);
    }
    if ( w <= 0 ) {
        printf("Invalid value for w.\n");
        exit(0);
    }
    if (w <= 16 && k + m > (1 << w)) {
        printf("k + m is too big.\n");
        exit(0);
    }
}

char *buffers[100];
int nextBufferIndex = 0;

char *allocateAligned(int size, char* alignmentTarget) {
    char* buffer = (char *)malloc(sizeof(char) * size + 16);
    buffers[nextBufferIndex] = buffer;
    nextBufferIndex++;

    int difference = buffer - alignmentTarget;
    if (difference % 16 != 0) {
        printf("target is %p, allocated %d bytes at %p, returning %p with offset %d\n",alignmentTarget,size, buffer, buffer + (16-difference%16)%16, (16-difference%16)%16);
        return buffer + (16 - difference % 16)%16;
    }
    else {
        return buffer;
    }
}

void freeAlignedBuffers() {
    int i;
    for (i = 0; i < nextBufferIndex; i++) {
        printf("%d free \n", i);
        free(buffers[i]);
    }
    nextBufferIndex = 0;
}

/* expected args: k, m, w, file size, file content */
static int encode (lua_State *L) {
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int size = luaL_checknumber(L, 4);
    char* content = luaL_checkstring(L, 5);
    check_args(k, m, w);
    printf("encode(k = %d, m = %d, w = %d, bytes-of-data = %d)\n\n", k, m, w, size);

    int i, j, l;
    int *matrix;
    char **data, **coding;
    int device_size, device_size_in_bytes;

    device_size = 1 + (size - 1) / (k * w/8); //commonDivisor; // round up
    int alignment = 16/(w/8);
    device_size = alignment * (device_size/alignment) + alignment;
    device_size_in_bytes = device_size * w/8;
    printf("device_size = %d (of %d bits each)\n", device_size, w);

    char** buffers = (char**)malloc(sizeof(char*) * (k + m));

    //printf("allocating %d bytes for each data[i]\n", ((int)sizeof(char) * device_size_in_bytes));
    data = (char**)malloc(sizeof(char*) * k);
    coding = (char **)malloc(sizeof(char*)*m);

    for (i = 0; i < m; i++) {
        coding[i] = allocateAligned(device_size_in_bytes, content);
    }

    /* fill in data devices; add padding to the last one, if necessary */
    for ( i = 0; i < k; i++ ) {
        int offsetInContent = i * device_size_in_bytes;
        printf("data[%d]: ", i);
        if (i < (size / device_size_in_bytes)) {
            //memcpy(data[i], content + offsetInContent, device_size_in_bytes);
            data[i] = content + offsetInContent;
            //printf("memcpy %d - %d \n", offsetInContent, offsetInContent + device_size_in_bytes );
        } else {
            int lastContentSize = (size - offsetInContent);
            int paddingSize;

            data[i] = allocateAligned(device_size_in_bytes,content);
            if (lastContentSize > 0) {
                paddingSize = device_size_in_bytes - lastContentSize;
                memcpy(data[i], content + offsetInContent, lastContentSize);
                memset(data[i] + lastContentSize, '0', paddingSize);
            } else {
                memset(data[i], '0', device_size_in_bytes);
            }
            //printf("mancpy %d - %d + padding %d\n", offsetInContent, offsetInContent + lastContentSize, paddingSize);
        }
    }

    print_data_and_coding(k, m, w, device_size_in_bytes, data, coding);

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

    jerasure_matrix_encode(k, m, w, matrix, data, coding, device_size_in_bytes);
    printf("\nEncoding Complete:\n\n");
    print_data_and_coding(k, m, w, device_size_in_bytes, data, coding);

    /* precede data and coding devices by their index (on DEVICE_INDEX_SIZE bytes) */
    for ( i = 0; i < k; i++ ) {
        char * dataDeviceArr = (char *) malloc(sizeof(char) * (sizeof(int) + device_size_in_bytes));
        memcpy(dataDeviceArr, &i, sizeof(int));
        memcpy(dataDeviceArr + sizeof(int), data[i], device_size_in_bytes);
        //printf("%d %d\n", i, (int)strlen(dataDeviceArr));
        lua_pushlstring(L, dataDeviceArr, device_size_in_bytes + sizeof(int));
        free(dataDeviceArr);
    }
    for ( i = 0; i < m; i++ ) {
        int index = i + k;
        char * codingDeviceArr = (char *) malloc(sizeof(char) * (device_size_in_bytes + sizeof(int)));
        memcpy(codingDeviceArr, &index, sizeof(int));
        memcpy(codingDeviceArr + sizeof(int), coding[i], device_size_in_bytes);
        //printf("%d %d\n", i + k, (int)strlen(codingDeviceArr));
        lua_pushlstring(L, codingDeviceArr, device_size_in_bytes + sizeof(int));
        free(codingDeviceArr);
    }

    freeAlignedBuffers();
    free(data);
    free(coding);

    return (k + m);
}

/* expected args: k, m, w, device_size, at least k devices */
static int decode (lua_State *L) {
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int device_size = luaL_checknumber(L, 4);
    check_args(k, m, w);
    printf("decode( k = %d, m = %d, w = %d, device_size = %d)\n\n", k, m, w, device_size);

    /* pop k, m, w, device_size from stack */
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);

    int num, i, j;
    int *erasures; /* ids of missing coded data; last elem is -1 */
    int *existing;
    int *matrix;
    char **data, **coding;
    int device_size_in_bytes = device_size * w/8;

    erasures = (int *)malloc(sizeof(int) * (k+m));
    existing = (int *)malloc(sizeof(int) * (k+m));
    for (i = 0; i < (k+m); i++) {
        existing[i] = 0;
    }
    int num_erasures = 0;

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);

    /*data = (char**)malloc(sizeof(char*)*k);
    for (i = 0; i < k; i++) {
        data[i] = (char *)malloc(sizeof(char) * device_size_in_bytes);
    }

    coding = (char **)malloc(sizeof(char*)*m);
    for (i = 0; i < m; i++) {
        coding[i] = (char *)malloc(sizeof(char)* device_size_in_bytes);
    }*/

    data = (char**)malloc(sizeof(char*) * k);
    coding = (char **)malloc(sizeof(char*) * m);

    char * alignmentTarget = NULL;
    while(lua_next(L, -2) != 0)
    {
        if(lua_isstring(L, -1)) {
            char* rawDataDevice = luaL_checkstring(L, -1);

            // first sizeof(int) positions represent the index of the data device
            int index;// = atoi(indexArr);
            memcpy(&index, rawDataDevice, sizeof(int));

            existing[index] = 1;

            if (alignmentTarget == NULL) {
                alignmentTarget = rawDataDevice + sizeof(int);
            }

            //printf("%d ", index);
            if (index < k) {
                //printf(" is data\n");
                //memcpy(data[index], rawDataDevice + sizeof(int), device_size_in_bytes);
                data[index] = rawDataDevice + sizeof(int);
            } else {
                //printf(" is coding\n");
                //memcpy(coding[index - k], rawDataDevice + sizeof(int), device_size_in_bytes);
                coding[index - k] = rawDataDevice + sizeof(int);
            }
        }
        lua_pop(L, 1);
    }

    for (i = 0; i < (k + m); i++) {
        if (existing[i] == 0) {
            //printf("data %d needs 0s\n", i);
            //memset(data[i], '0', device_size_in_bytes);
            //printf("%d ", i);
            if (i < k) {
                //printf(" is data filled with 0s \n");
                data[i] = allocateAligned(device_size_in_bytes,alignmentTarget);
                memset(data[i], '0', device_size_in_bytes);
            } else {
                //printf(" is coding filled with 0s \n");
                printf("i = %d i-k = %d \n", i, (i-k));
                coding[i - k] = allocateAligned(device_size_in_bytes,alignmentTarget);
                printf("%p\n", coding[i-k]);
                memset(coding[i - k], '0', device_size_in_bytes);
            }
        }
    }

    for (i = 0; i < (k + m); i++) {
        if (existing[i] == 0) {
            erasures[num_erasures] = i;
            num_erasures++;
        }
    }
    erasures[num_erasures] = -1;
    num_erasures++;

    /*for (i = 0; i < num_erasures; i++) {
        printf("%d \n", erasures[i]);
    }*/

    print_data_and_coding(k, m, w, device_size_in_bytes, data, coding);

    //printf("Missing coded blocks: ");
    /*for (i = 0 ; i < num_erasures; i++) {
        printf("%d ", erasures[i]);
    }
    printf("\n\n");*/

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    i = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data, coding, device_size_in_bytes);

    printf("State of the system after decoding:\n\n");
    print_data_and_coding(k, m, w, device_size_in_bytes, data, coding);


    char *content = (char*) malloc(sizeof(char) * device_size_in_bytes * k);
    for ( i = 0; i < k; i++ ) {
        //char * dataDeviceArr = (char *) malloc(sizeof(char) * device_size_in_bytes);
        memcpy(content + (i * device_size_in_bytes), data[i], device_size_in_bytes);
    }
    lua_pushlstring(L, content, device_size_in_bytes * k);

    free(existing);
    free(erasures);
    freeAlignedBuffers();
    free(data);
    free(coding);

    return 1;
}

int luaopen_luajerasure(lua_State *L) {
    lua_register(
            L,               /* Lua state variable */
            "encode",        /* func name as known in Lua */
            encode          /* func name in this file */
            );  
    lua_register(L,"decode",decode);
    return 2;
}