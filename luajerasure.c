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
  fprintf(stderr, "usage: reed_sol_01 k m w seed - Does a simple Reed-Solomon coding example in GF(2^w).\n");
  fprintf(stderr, "       \n");
  fprintf(stderr, "w must be 8, 16 or 32.  k+m must be <= 2^w.  It sets up a classic\n");
  fprintf(stderr, "Vandermonde-based generator matrix and encodes k devices of\n");
  fprintf(stderr, "%ld bytes each with it.  Then it decodes.\n", sizeof(long));
  fprintf(stderr, "       \n");
  fprintf(stderr, "This demonstrates: jerasure_matrix_encode()\n");
  fprintf(stderr, "                   jerasure_matrix_decode()\n");
  fprintf(stderr, "                   jerasure_print_matrix()\n");
  fprintf(stderr, "                   reed_sol_vandermonde_coding_matrix()\n");
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

static int encode (lua_State *L) {
    int k = lua_tonumber(L, 1);
    int m = lua_tonumber(L, 2);
    int w = lua_tonumber(L, 3);
    printf("encode(k = %d, m = %d, w = %d)\n\n", k, m, w);

    int i, j;
    int *matrix;
    char **data, **coding;
    unsigned char uc;
    int *erasures;
    uint32_t seed = 105; // TODO
    if (w <= 16 && k + m > (1 << w)) usage("k + m is too big");
    
    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

    printf("Last m rows of the generator Matrix (G^T):\n");
    jerasure_print_matrix(matrix, m, k, w);
    //printf("\n");

    MOA_Seed(seed);
    data = talloc(char *, k);
    for (i = 0; i < k; i++) {
        data[i] = talloc(char, sizeof(long));
        for (j = 0; j < sizeof(long); j++) {
            uc = MOA_Random_W(8, 1);
            data[i][j] = (char) uc;
        }
    }

    coding = talloc(char *, m);
    for (i = 0; i < m; i++) {
        coding[i] = talloc(char, sizeof(long));
    }

    jerasure_matrix_encode(k, m, w, matrix, data, coding, sizeof(long));

    printf("\nEncoding Complete:\n\n");
    print_data_and_coding(k, m, w, sizeof(long), data, coding);

    int x, n;
    int size = sizeof(long);
    int sp = size * 2 + size/(w/8) + 8;
    
    size_t nbytes = m * sizeof(int); // num of bytes for coding array

    for ( i = 0; i < k; i++ ) {
        lua_createtable(L, k, 0);
        lua_pushnumber(L, i);
        lua_rawseti (L, -2, 1);
        for( j = 0; j < size; j += (w/8) ) {
            for( x = 0; x < w/8; x++ ) {
                //printf("x%d",x); //printf("%02x", (unsigned char)data[i][j+x]);
                lua_pushnumber(L, (unsigned char)data[i][j+x]);
            }
            lua_rawseti (L, -2, j + 2);
        }
        //printf("%d ", i);
    }

    for (i = 0 ; i < m ; i++ ) {
        lua_createtable(L, m, 0);
        lua_pushnumber(L, k + i);
        lua_rawseti (L, -2, 1);
        for( j = 0; j < size; j += (w/8) ) {
            for( x = 0; x < w/8; x++ ) {
                //printf("x%d",x);//printf("%02x", (unsigned char)coding[i][j+x]);
                lua_pushnumber(L, (unsigned char)coding[i][j+x]);
            }
            lua_rawseti (L, -2, j + 2);
        }
    }

    return (k + m);
}

static int decode (lua_State *L) {
    int k = lua_tonumber(L, 1);
    int m = lua_tonumber(L, 2);
    int w = lua_tonumber(L, 3);
    printf("decode( k = %d, m = %d, w = %d)\n\n", k, m, w);
    lua_remove(L,1);
    lua_remove(L,1);
    lua_remove(L,1);

    luaL_checktype(L, 1, LUA_TTABLE);
    //printf("%s\n", lua_typename(L, lua_type(L, 1)));
    //printf("%s\n", lua_typename(L, lua_type(L, 2)));
    //printf("%s\n", lua_typename(L, lua_type(L, 3)));
    //printf("%s\n", lua_typename(L, lua_type(L, 4)));

    int num;
    int size = sizeof(long);
    int i, j;

    int *erasures = talloc(int, (m+1));
    int *matrix;
    char **data, **coding;
    unsigned char uc;
    if (w <= 16 && k + m > (1 << w)) usage("k + m is too big");

    data = talloc(char *, k);
    for (i = 0; i < k; i++) {
        data[i] = talloc(char, sizeof(long));
    }
    coding = talloc(char *, m);
    for (i = 0; i < m; i++) {
        coding[i] = talloc(char, sizeof(long));
    }

    // erasures = ids of missing coded data; last elem is -1
    erasures = talloc(int, (k+m));
    int* existing = talloc(int, (k+m));
    for (i = 0; i < (k+m); i++) {
        existing[i] = 0;
    }
    int num_erasures = 0;

    for(i = 0; i < k+m; i++) {
        lua_pushinteger(L, i+1);
        lua_gettable(L, 1);
        if (lua_istable(L, -1)) {
            //printf("\nA table!\n");
            for (j=0; j<size+1; j++) {
                lua_rawgeti(L, i+2, j+1);
                if (lua_isnumber(L, -1)) {
                    int crt_line;
                    if ( j == 0 ) {
                        crt_line = (int)lua_tonumber(L, -1);
                        //printf("%d ", crt_line);
                        existing[crt_line] = 1;
                    } else { // if j>0
                        int encoded_data = (int)lua_tonumber(L, -1);
                        //printf("%02x ", encoded_data);
                        if (crt_line < k ) {
                            data[crt_line][j-1] = encoded_data;
                        } else {
                            coding[crt_line-k][j-1] = encoded_data;
                        }
                    }
                }
                lua_pop(L, 1);
                }
        }
        //printf("\n");
    }

    for (i = 0; i < (k + m); i++) {
        if (existing[i] == 0) {
            erasures[num_erasures] = i;
            num_erasures++;
        }
    }

    erasures[num_erasures] = -1;
    num_erasures++;
    print_data_and_coding(k, m, w, sizeof(long), data, coding);
    printf("Missing coded blocks: ");
    for (i = 0 ; i < num_erasures; i++) {
        printf("%d ", erasures[i]);
    }
    printf("\n\n");

    matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    i = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data, coding, sizeof(long));

    printf("State of the system after decoding:\n\n");
    print_data_and_coding(k, m, w, sizeof(long), data, coding);

    return 0;
}

static int test(lua_State *L) {
    //luaL_checktype(L, 1, LUA_TTABLE);
    //int *buff = (int*)malloc(5*sizeof(int));

    int num = lua_tonumber(L, 1);
    printf("num=%d\n", num);
    printf("%s\n", lua_typename(L, lua_type(L, 1)));
    printf("%s\n", lua_typename(L, lua_type(L, 2)));
    lua_remove(L,1);
    printf("%s\n", lua_typename(L, lua_type(L, 1)));
    printf("%s\n", lua_typename(L, lua_type(L, 2)));

     int i, j;
     for(i = 0; i < 10; i++) {
        lua_pushinteger(L, i+1);
        lua_gettable(L, 1);

        if (lua_istable(L, -1)) {
            printf("\nA table!\n");
            for (j=0; j<5; j++) {
                lua_rawgeti(L, i+2, j+1);
                if (lua_isnumber(L, -1))
                    printf("num%d\n",(int)lua_tonumber(L, -1));
                lua_pop(L, 1);
            }

        }
    }

    /*lua_pushnil(L);
    int i =lua_gettop(L);
    while (lua_next(L, i != 0)) {
        printf("%d", i);
       printf("%s - %s\n",
              lua_typename(L, lua_type(L, -2)),
              lua_typename(L, lua_type(L, -1)));
       lua_pop(L, 1);
     }*/
    return 0;
}

// get a table from lua x={1,2,3,4,5}
/*static int test(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int *buff = (int*)malloc(5*sizeof(int));

    int i;
     for(i = 0; i < 5; i++) {
        lua_pushinteger(L, i+1);
        lua_gettable(L, 1);
        if(lua_isnumber(L, -1)) {
            buff[i] = lua_tonumber(L, -1);
            printf("%d ", buff[i]);
        } else {
            lua_pushfstring(L,
                strcat(
                    strcat(
                        "invalid entry #%d in array argument #%d (expected number, got ",
                        luaL_typename(L, -1)
                    ),
                    ")"
                )
            );
            lua_error(L);
        }
        lua_pop(L, 1);
    }

    return 1;
}*/

int luaopen_luajerasure(lua_State *L) {
    lua_register(
            L,               /* Lua state variable */
            "encode",        /* func name as known in Lua */
            encode          /* func name in this file */
            );  
    lua_register(L,"decode",decode);
    lua_register(L, "test", test);
    return 3;
}
