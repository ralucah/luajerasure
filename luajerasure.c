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

#define talloc(type, num) (type *) malloc(sizeof(type)*(num))

usage(char *s)
{
    fprintf(stderr, "Lua binding for a simple Reed-Solomon coding example in GF(2^w).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "w must be 8, 16 or 32.  k+m must be <= 2^w.\n");
    if (s != NULL) fprintf(stderr, "%s\n", s);
    exit(1);
}

static void print_data_and_coding(int k, int m, int w, int size, char **data, char **coding) 
{
    int i, j, x;
    int n, sp;
    long l;

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
        printf("Invalid value for k!\n");
        exit(0);
    }
    if ( m <= 0 ) {
        printf("Invalid value for m!\n");
        exit(0);
    }
    if ( w <= 0 ) {
        printf("Invalid value for w!\n");
        exit(0);
    }

    if (w <= 16 && k + m > (1 << w)) {
        printf("k + m is too big");
        exit(0);
    }
}

static int encode (lua_State *L) {
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int fileSize = luaL_checknumber(L, 4);
    char* fileData = luaL_checkstring(L, 5);
    printf("encode(k = %d, m = %d, w = %d, bytes-of-data = %d)\n\n", k, m, w, fileSize);
    //check_args(k, m, w);

    int i, j, l, n, x;
    int *matrix;
    char **data, **coding;
    int size;
    int blocksize;

    /* use file data to fill in the data matrix; assuming w = 8 */
    int dataDeviceSize = 1 + ((fileSize - 1) / k); // round up
    printf("%d data devices of %d bytes each\n", k, dataDeviceSize);

    data = (char**)malloc(sizeof(char*)*k);
    for (i = 0; i < k; i++) {
        data[i] = (char *)malloc(sizeof(char) * dataDeviceSize);
    }

    for ( i = 0; i < k; i++ ) {
        //printf("%d data:%d-%d ", i, i * dataDeviceSize, (i+1) * dataDeviceSize);
        //printf("\ni=%d\n",i);

        for ( j = i * dataDeviceSize; j < ((i+1) * dataDeviceSize); j++ ) {
            l = j % dataDeviceSize;
            if (j < fileSize) {
                data[i][l] = fileData[j];
            } else {
                data[i][l] = '0';
            }
            //printf("(%d=%c) ", j, data[i][l]);
        }
        //printf("data[%d]: strlen:%d, data:%s \n", i, (int)strlen(data[i]), data[i]);
        //printf("data[%d]: strlen:%d \n", i, (int)strlen(data[i]));
        //printf("\n");
    }

    coding = (char **)malloc(sizeof(char*)*m);
    for (i = 0; i < m; i++) {
        coding[i] = (char *)malloc(sizeof(char)*dataDeviceSize);
                if (coding[i] == NULL) { perror("malloc"); exit(1); }
    }

    printf("\n");
    print_data_and_coding(k, m, w, dataDeviceSize, data, coding);

    /* create coding matrix */
    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

    /* encode */
    jerasure_matrix_encode(k, m, w, matrix, data, coding, dataDeviceSize);

    printf("\nEncoding Complete:\n\n");
    print_data_and_coding(k, m, w, dataDeviceSize, data, coding);

    for ( i = 0; i < k; i++ ) {
        // TODO always allocate 5 bytes for the index
        char * indexArr = (char *) malloc(sizeof(char) * 5);
        sprintf(indexArr, "%05d", i);
        //printf("%s of strlen %d \n", indexArr, (int)(strlen(indexArr)));

        char * dataDeviceArr = (char *) malloc(sizeof(char) * (5 + dataDeviceSize + 1)); // +1?
        dataDeviceArr[0] = '\0';
        memcpy(dataDeviceArr, indexArr, 5);
        memcpy(dataDeviceArr + 5, data[i], dataDeviceSize);
        //printf("%d %d\n", i, (int)strlen(dataDeviceArr));
        lua_pushlstring(L, dataDeviceArr, dataDeviceSize + 5);
    }

    for ( i = 0; i < m; i++ ) {
        // TODO always allocate 5 bytes for the index
        char * indexArr = (char *) malloc(sizeof(char) * 5);
        sprintf(indexArr, "%05d", i + k);
        //printf("%s of strlen %d \n", indexArr, (int)(strlen(indexArr)));

        char * codingDeviceArr = (char *) malloc(sizeof(char) * (5 + dataDeviceSize + 1)); // +1?
        codingDeviceArr[0] = '\0';
        memcpy(codingDeviceArr, indexArr, 5);
        memcpy(codingDeviceArr + 5, coding[i], dataDeviceSize);
        //printf("%d %d\n", i + k, (int)strlen(codingDeviceArr));
        lua_pushlstring(L, codingDeviceArr, dataDeviceSize + 5);
    }

    return (k + m);
}

static int decode (lua_State *L) {
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int dataDeviceSize = luaL_checknumber(L, 4);
    printf("decode( k = %d, m = %d, w = %d, dataDeviceSize = %d)\n\n", k, m, w, dataDeviceSize);

    /* pop k, m, w, dataDeviceSize from stack */
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);

    int num, i, j;
    int *erasures; //  ids of missing coded data; last elem is -1
    int *existing;
    int *matrix;
    char **data, **coding;
    unsigned char uc;
    if (w <= 16 && k + m > (1 << w)) usage("k + m is too big");

    data = (char**)malloc(sizeof(char*)*k);
    for (i = 0; i < k; i++) {
        data[i] = (char *)malloc(sizeof(char) * dataDeviceSize);
    }

    coding = (char **)malloc(sizeof(char*)*m);
    for (i = 0; i < m; i++) {
        coding[i] = (char *)malloc(sizeof(char)* dataDeviceSize);
    }

    erasures = (int *)malloc(sizeof(int) * (k+m));
    existing = (int *)malloc(sizeof(int) * (k+m));
    for (i = 0; i < (k+m); i++) {
        existing[i] = 0;
    }
    int num_erasures = 0;

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);

    while(lua_next(L, -2) != 0)
    {
        if(lua_isstring(L, -1)) {
            char* rawDataDevice = luaL_checkstring(L, -1);

            // first 5 positions represent the index of the data device
            char * indexArr = (char *) malloc(sizeof(char) * 5);
            memcpy(indexArr, rawDataDevice, 5);
            int index = atoi(indexArr);

            existing[index] = 1;

            //printf("%d ", index);
            if (index < k) {
                //printf(" is data\n");
                memcpy(data[index], rawDataDevice + 5, dataDeviceSize);
            } else {
                //printf(" is coding\n");
                memcpy(coding[index - k], rawDataDevice + 5, dataDeviceSize);
            }
        }
        lua_pop(L, 1);
    }

    for (i = 0; i < k; i++) {
        if (!data[i][0]) {
            //printf("data %d needs 0s\n", i);
            for (j = 0; j < dataDeviceSize; j++) {
                data[i][j] = 0;
            }
        }
    }

    for (i = 0; i < m; i++) {
        if (!coding[i][0]) {
            //printf("coding %d needs 0s\n", i);
            for (j = 0; j < dataDeviceSize; j++) {
                coding[i][j] = 0;
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

    print_data_and_coding(k, m, w, dataDeviceSize, data, coding);
    printf("Missing coded blocks: ");
    for (i = 0 ; i < num_erasures; i++) {
        printf("%d ", erasures[i]);
    }
    printf("\n\n");

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    i = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data, coding, dataDeviceSize);

    printf("State of the system after decoding:\n\n");
    print_data_and_coding(k, m, w, dataDeviceSize, data, coding);

    return 0;
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