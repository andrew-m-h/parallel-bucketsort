#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef int elem_t;

int main(int argc, char* argv[]){
    size_t len, i;
    elem_t* buff;
    FILE* rand_source = fopen("/dev/urandom", "r");

    if (argc != 2){
        perror("Must pass length argument");
        return EXIT_SUCCESS;
    }
    len = strtoull(argv[1], NULL, 10);

    buff = (elem_t*)malloc(len * sizeof(elem_t));
    assert(buff != NULL);

    fread((void*)buff, sizeof(elem_t), len, rand_source);
    fclose(rand_source);

    for (i=0; i < len; i++){
        printf("%d\n", buff[i]);
    }

    free(buff);

    return EXIT_SUCCESS;
}
