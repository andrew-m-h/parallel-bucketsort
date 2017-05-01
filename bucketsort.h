#ifndef BUCKETSORT_H
#define BUCKETSORT_H

#include <stdlib.h>
#include <stdio.h>
#include <iso646.h>

#include <mpi.h>

/*in case ssize_t is not present nativly*/
#if !defined(ssize_t)
typedef signed long long int ssize_t;
#endif

/*elem_t is the type to sort*/
typedef int elem_t;

/*MPI types to send standard types in messages*/
#define MPI_SIZE_T MPI_UNSIGNED_LONG_LONG
#define MPI_ELEM_T MPI_INT

#define MAIN_RANK 0

/*arbitrary tag values*/
#define LENGTH_TAG 0x42
#define DATA_TAG   0x73
#define SIGNAL_TAG 0x123

/*global variables*/
static int rank, size;
static size_t big_bucket_length;
static elem_t * big_bucket;
static size_t input_length;

static elem_t min_max[2];
#define max_value (min_max[1])
#define min_value (min_max[0])

static inline elem_t from_string(const char*);
static inline void print_elem_ln(FILE*, elem_t);
static inline void scanf_elem(FILE*, elem_t*);
static int gt(const void*, const void*);

#if defined(DEBUG) or defined(CHECK_MALLOC)
static inline void check_malloc(void *, char*);
#endif

/*retrieve an elem_t from a string using the appropriate stdlib function*/
static inline elem_t from_string(const char* s){
    return atoi(s);
}

/*write a value, v to a file with a trailing new line*/
static inline void print_elem_ln(FILE* fp, elem_t v){
    fprintf(fp, "%d\n", v);
}

/*get an elem_t from a file*/
static inline void scanf_elem(FILE* fp, elem_t* res){
    fscanf(fp, "%d", res);
}

/*gt function to use with qsort. Has fast path for int.*/
static int gt(const void* a, const void* b){
    const ssize_t a_val = (ssize_t)*(elem_t*)a;
    const ssize_t b_val = (ssize_t)*(elem_t*)b;

    const ssize_t diff = a_val - b_val;

    if (diff > 0) return 1;
    else if (diff < 0) return -1;
    else return 0;
}

/*check memory allocations and print debugging information*/
#if defined(DEBUG) or defined(CHECK_MALLOC)
static inline void check_malloc(void * m, char* s){
    if (m == NULL){
        fprintf(stderr, "Procecss %d\n%s\nMalloc error\n", rank, s);
        exit(EXIT_FAILURE);
    }
}
#else
#define check_malloc(m, s) ;
#endif

#endif
