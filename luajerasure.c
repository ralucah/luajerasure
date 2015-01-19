/**
* Lua binding for Jerasure's Vandermonde Reed-Solomon.
* w must be 8,16,32
* k + m must be <= 2^w
*/

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

#include "data_and_coding_printer.h"

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

/* allocate memory such that data and coding devices are aligned */
char *allocateAligned(int size, char* alignmentTarget) {
    char* buffer = (char *)malloc(sizeof(char) * size + 16);
    buffers[nextBufferIndex] = buffer;
    nextBufferIndex++;

    int difference = buffer - alignmentTarget;
    if (difference % 16 != 0) {
        return buffer + (16 - difference % 16)%16;
    }
    else {
        return buffer;
    }
}

/* free memory allocated for aligned data and coding devices */
void freeAlignedBuffers() {
    int i;
    for (i = 0; i < nextBufferIndex; i++) {
        free(buffers[i]);
    }
    nextBufferIndex = 0;
}

/* wrapper for jerasure's encode function */
static int encode (lua_State *L) {
    /* get k, m, w, file size, file content from the stack */
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int size = luaL_checknumber(L, 4);
    char* content = luaL_checkstring(L, 5);
    check_args(k, m, w);
    //printf("encode(k = %d, m = %d, w = %d, bytes-of-data = %d)\n\n", k, m, w, size);

    int i, j, l;
    int *matrix;
    char **data, **coding;
    int deviceSize, deviceSizeInBytes, alignment;
    char** buffers;

    deviceSize = 1 + (size - 1) / (k * w/8); // size / (k * w/8) round up
    alignment = 16/(w/8);
    deviceSize = alignment * (deviceSize/alignment) + alignment;
    deviceSizeInBytes = deviceSize * w/8;

    buffers = (char**)malloc(sizeof(char*) * (k + m));

    data = (char**)malloc(sizeof(char*) * k);
    coding = (char **)malloc(sizeof(char*)*m);

    for (i = 0; i < m; i++) {
        coding[i] = allocateAligned(deviceSizeInBytes, content);
    }

    /* fill in data devices; add padding, if necessary */
    for ( i = 0; i < k; i++ ) {
        int offsetInContent = i * deviceSizeInBytes;
        if (i < (size / deviceSizeInBytes)) {
            data[i] = content + offsetInContent;
        } else {
            int lastContentSize = (size - offsetInContent);
            int paddingSize;

            data[i] = allocateAligned(deviceSizeInBytes,content);
            if (lastContentSize > 0) {
                paddingSize = deviceSizeInBytes - lastContentSize;
                memcpy(data[i], content + offsetInContent, lastContentSize);
                memset(data[i] + lastContentSize, '0', paddingSize);
            } else {
                memset(data[i], '0', deviceSizeInBytes);
            }
        }
    }

    //printDataAndCoding(k, m, w, deviceSizeInBytes, data, coding);

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

    jerasure_matrix_encode(k, m, w, matrix, data, coding, deviceSizeInBytes);
    //printf("\nEncoding Complete:\n\n");
    //printDataAndCoding(k, m, w, deviceSizeInBytes, data, coding);

    /* precede data and coding devices by their index (on sizeof(int) bytes) */
    for ( i = 0; i < k; i++ ) {
        char * dataDeviceArr = (char *) malloc(sizeof(char) * (sizeof(int) + deviceSizeInBytes));
        memcpy(dataDeviceArr, &i, sizeof(int));
        memcpy(dataDeviceArr + sizeof(int), data[i], deviceSizeInBytes);
        lua_pushlstring(L, dataDeviceArr, deviceSizeInBytes + sizeof(int));
        free(dataDeviceArr);
    }
    for ( i = 0; i < m; i++ ) {
        int index = i + k;
        char * codingDeviceArr = (char *) malloc(sizeof(char) * (deviceSizeInBytes + sizeof(int)));
        memcpy(codingDeviceArr, &index, sizeof(int));
        memcpy(codingDeviceArr + sizeof(int), coding[i], deviceSizeInBytes);
        lua_pushlstring(L, codingDeviceArr, deviceSizeInBytes + sizeof(int));
        free(codingDeviceArr);
    }

    freeAlignedBuffers();
    free(data);
    free(coding);

    return (k + m);
}

/* wrapper for jerasure's encode function */
static int decode (lua_State *L) {
    /* get k, m, w, deviceSize, and devices (should be at least k) from the stack */
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int deviceSize = luaL_checknumber(L, 4);
    check_args(k, m, w);
    //printf("decode( k = %d, m = %d, w = %d, deviceSize = %d)\n\n", k, m, w, deviceSize);

    /* pop k, m, w, deviceSize from stack */
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);

    int num, i, j;
    int *erasures; /* ids of missing coded data; last elem is -1 */
    int *existing;
    int *matrix;
    char **data, **coding;
    int deviceSizeInBytes = deviceSize * w/8;
    int numErasures = 0;

    erasures = (int *)malloc(sizeof(int) * (k+m));
    existing = (int *)malloc(sizeof(int) * (k+m));
    for (i = 0; i < (k+m); i++) {
        existing[i] = 0;
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);

    data = (char**)malloc(sizeof(char*) * k);
    coding = (char **)malloc(sizeof(char*) * m);

    /* get devices from the stack */
    char * alignmentTarget = NULL;
    while(lua_next(L, -2) != 0)
    {
        if(lua_isstring(L, -1)) {
            char* rawDataDevice = luaL_checkstring(L, -1);

            /* first sizeof(int) positions represent the index of the data device */
            int index;
            memcpy(&index, rawDataDevice, sizeof(int));

            existing[index] = 1;

            if (alignmentTarget == NULL) {
                alignmentTarget = rawDataDevice + sizeof(int);
            }

            if (index < k) {
                data[index] = rawDataDevice + sizeof(int);
            } else {
                coding[index - k] = rawDataDevice + sizeof(int);
            }
        }
        lua_pop(L, 1);
    }

    /* fill in with '0' the missing data and coding devices */
    for (i = 0; i < (k + m); i++) {
        if (existing[i] == 0) {
            if (i < k) {
                data[i] = allocateAligned(deviceSizeInBytes,alignmentTarget);
                memset(data[i], '0', deviceSizeInBytes);
            } else {
                coding[i - k] = allocateAligned(deviceSizeInBytes,alignmentTarget);
                memset(coding[i - k], '0', deviceSizeInBytes);
            }
        }
    }

    /* store indices of missing devices in erasures */
    for (i = 0; i < (k + m); i++) {
        if (existing[i] == 0) {
            erasures[numErasures] = i;
            numErasures++;
        }
    }

    /* last element of erasures must be -1*/
    erasures[numErasures] = -1;
    numErasures++;

    //printDataAndCoding(k, m, w, deviceSizeInBytes, data, coding);

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    i = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data, coding, deviceSizeInBytes);

    //printf("State of the system after decoding:\n\n");
    //printDataAndCoding(k, m, w, deviceSizeInBytes, data, coding);

    /* push decoded data devices on the stack */
    char *content = (char*) malloc(sizeof(char) * deviceSizeInBytes * k);
    for ( i = 0; i < k; i++ ) {
        memcpy(content + (i * deviceSizeInBytes), data[i], deviceSizeInBytes);
    }
    lua_pushlstring(L, content, deviceSizeInBytes * k);

    free(existing);
    free(erasures);
    freeAlignedBuffers();
    free(data);
    free(coding);

    return 1;
}

static const struct luaL_Reg luajerasure_funcs[] = {
    { "encode", encode},
    { "decode", decode },
    {NULL, NULL}
};

int luaopen_luajerasure(lua_State *L) {
    luaL_openlib(L, "luajerasure", luajerasure_funcs, 0);

    return 1;
}