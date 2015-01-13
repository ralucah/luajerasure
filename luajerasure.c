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
    fprintf(stderr, "w must be 8, 16 or 32. k+m must be <= 2^w.\n");
    if (s != NULL) fprintf(stderr, "%s\n", s);
    exit(1);
}

static void print_data_and_coding(int k, int m, int w, int size, 
        char **data, char **coding) 
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
    int k = luaL_checknumber(L, 1);         /* number of data devices */
    int m = luaL_checknumber(L, 2);         /* number of coding devices*/
    int w = luaL_checknumber(L, 3);         /* TODO word size (8, 16 or 32) */

    int size = luaL_checknumber(L, 4);      /* file size in bytes */
    char* content = luaL_checkstring(L, 5); /* file content */
    printf("encode(k = %d, m = %d, w = %d, bytes-of-data = %d)\n\n", k, m, w, size);
    // TODO check args

    int i, j, l;                            /* iterators */
    int *matrix;                            /* Vandermonde matix */
    char **data, **coding;                  /* data and coding devices */
    int newsize;                            /* new file size that is a multiple of k * (w/8)*/
    int deviceSize;

    /* file size needs to be a multiple of k * (w/8) */
    deviceSize = 1 + ((size - 1) / (k * (w / 8))); // round up

    printf("deviceSize = %d (of %d bits each)\n", deviceSize, w);
    newsize = deviceSize * (k * (w / 8));
    printf("newsize = %d \n", newsize);

    /* allocate memory for data devices */
    printf("allocating %d bytes for each data[i]\n", ((int)sizeof(char) * w/8 * deviceSize));
    data = (char**)malloc(sizeof(char*) * k);
    for (i = 0; i < k; i++) {
        data[i] = (char *)malloc(sizeof(char) * w/8 * deviceSize);
        //if (data[i] == NULL) { perror("data malloc"); exit(1); }
    }

    /* fill in data devices; add padding to the last one, if necessary */
    for ( i = 0; i < k; i++ ) {
        int offsetInContent = i * deviceSize * w/8;
        printf("data[%d]: ", i);
        if (i < (k - 1)) {
            memcpy(data[i], content + offsetInContent, deviceSize *w/8);
            //data[i] = content + offsetInContent;
            printf("memcpy %d - %d \n", offsetInContent, offsetInContent + (deviceSize * w/8) );
        } else {
            unsigned int lastContentSize = (size - offsetInContent);
            unsigned int paddingSize = deviceSize * w/8 - lastContentSize;
            memcpy(data[i], content + offsetInContent, lastContentSize);
            memset(data[i] + lastContentSize, '0', paddingSize);
            printf("mancpy %d - %d + padding %d\n", offsetInContent, offsetInContent + lastContentSize, paddingSize);
        }
    }

    /* allocate memory for coding devices */
    coding = (char **)malloc(sizeof(char*)*m);
    for (i = 0; i < m; i++) {
        coding[i] = (char *)malloc(sizeof(char) * w/8 * deviceSize);
        //if (coding[i] == NULL) { perror("coding malloc"); exit(1); }
    }

    print_data_and_coding(k, m, w, deviceSize * w/8, data, coding);

    /* create coding matrix */
    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

    /* encode */
    jerasure_matrix_encode(k, m, w, matrix, data, coding, deviceSize * w/8);

    printf("\nEncoding Complete:\n\n");
    print_data_and_coding(k, m, w, deviceSize * w/8, data, coding);

    for ( i = 0; i < k; i++ ) {
        // TODO always allocate 5 bytes for the index
        char * indexArr = (char *) malloc(sizeof(char) * 5);
        sprintf(indexArr, "%05d", i);
        //printf("%s of strlen %d \n", indexArr, (int)(strlen(indexArr)));

        char * dataDeviceArr = (char *) malloc(sizeof(char) * (5 + deviceSize * w/8 + 1)); // +1?
        dataDeviceArr[0] = '\0';
        memcpy(dataDeviceArr, indexArr, 5);
        memcpy(dataDeviceArr + 5, data[i], deviceSize * w/8);
        //printf("%d %d\n", i, (int)strlen(dataDeviceArr));
        lua_pushlstring(L, dataDeviceArr, deviceSize * w/8 + 5);
    }

    for ( i = 0; i < m; i++ ) {
        // TODO always allocate 5 bytes for the index
        char * indexArr = (char *) malloc(sizeof(char) * 5);
        sprintf(indexArr, "%05d", i + k);
        //printf("%s of strlen %d \n", indexArr, (int)(strlen(indexArr)));

        char * codingDeviceArr = (char *) malloc(sizeof(char) * (5 + deviceSize * w/8 + 1)); // +1?
        codingDeviceArr[0] = '\0';
        memcpy(codingDeviceArr, indexArr, 5);
        memcpy(codingDeviceArr + 5, coding[i], deviceSize * w/8);
        //printf("%d %d\n", i + k, (int)strlen(codingDeviceArr));
        lua_pushlstring(L, codingDeviceArr, deviceSize * w/8 + 5);
    }

    return (k + m);
}

static int decode (lua_State *L) {
    int k = luaL_checknumber(L, 1);
    int m = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int deviceSize = luaL_checknumber(L, 4);
    printf("decode( k = %d, m = %d, w = %d, deviceSize = %d)\n\n", k, m, w, deviceSize);

    /* pop k, m, w, deviceSize from stack */
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);

    int num, i, j;
    int *erasures; //  ids of missing coded data; last elem is -1
    int *existing;
    int *matrix;
    char **data, **coding;
    //if (w <= 16 && k + m > (1 << w)) usage("k + m is too big"); //TODO

    data = (char**)malloc(sizeof(char*)*k);
    for (i = 0; i < k; i++) {
        data[i] = (char *)malloc(sizeof(char) * deviceSize * w/8);
    }

    coding = (char **)malloc(sizeof(char*)*m);
    for (i = 0; i < m; i++) {
        coding[i] = (char *)malloc(sizeof(char)* deviceSize * w/8);
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
            //printf("A string! \n");
            char* rawDataDevice = luaL_checkstring(L, -1);

            // first 5 positions represent the index of the data device
            char * indexArr = (char *) malloc(sizeof(char) * 5);
            memcpy(indexArr, rawDataDevice, 5);
            int index = atoi(indexArr);

            existing[index] = 1;

            printf("%d ", index);
            if (index < k) {
                printf(" is data\n");
                memcpy(data[index], rawDataDevice + 5, deviceSize * w/8);
            } else {
                printf(" is coding\n");
                memcpy(coding[index - k], rawDataDevice + 5, deviceSize * w/8);
            }
        }
        lua_pop(L, 1);
    }

    for (i = 0; i < k; i++) {
        if (!data[i][0]) {
            //printf("data %d needs 0s\n", i);
            for (j = 0; j < deviceSize * w/8; j++) {
                data[i][j] = 0;
            }
        }
    }

    for (i = 0; i < m; i++) {
        if (!coding[i][0]) {
            //printf("coding %d needs 0s\n", i);
            for (j = 0; j < deviceSize * w/8; j++) {
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

    print_data_and_coding(k, m, w, deviceSize * w/8, data, coding);

    printf("Missing coded blocks: ");
    for (i = 0 ; i < num_erasures; i++) {
        printf("%d ", erasures[i]);
    }
    printf("\n\n");

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    i = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data, coding, deviceSize * w/8);

    printf("State of the system after decoding:\n\n");
    print_data_and_coding(k, m, w, deviceSize * w/8, data, coding);

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