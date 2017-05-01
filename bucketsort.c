#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <iso646.h>

#include <mpi.h>

#include "bucketsort.h"

static inline void range(size_t, elem_t*, elem_t[2]);
static inline void split_small_buckets(size_t*, elem_t*);

#ifdef OUTPUT
static inline void output_sorted(size_t, elem_t*);
#endif

#ifdef DEBUG
static void debug_1();
static void debug_2(size_t, elem_t*);
#endif

int main(int argc, char* argv[]){
    size_t i, j;
    elem_t * input_buffer = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc not_eq 2){
        perror("Must provide argument");
        exit(EXIT_SUCCESS);
        MPI_Finalize();
    }

    input_length = from_string(argv[1]);

    big_bucket_length = (rank == 0) ? (input_length/size + input_length % size)
        : (input_length / size);
    big_bucket = (elem_t*)malloc(big_bucket_length * sizeof(elem_t));
    check_malloc(big_bucket, "big_bucket");

    if (rank == MAIN_RANK){
        input_buffer = (elem_t*)malloc(input_length * sizeof(elem_t));
        check_malloc(input_buffer, "input_buffer");

        for (i=0; i < input_length; i++)
            scanf_elem(stdin, input_buffer+i);

        memcpy(big_bucket+(input_length/size), input_buffer,
               (input_length%size)*sizeof(elem_t));
        range(input_length, input_buffer, min_max);
    }

    /*Phase 1 - Send big buckets to each process*/
    MPI_Scatter(input_buffer + (input_length%size), input_length/size,
                MPI_ELEM_T, big_bucket, input_length/size,
                MPI_ELEM_T, MAIN_RANK, MPI_COMM_WORLD);
    MPI_Bcast(&min_max, 2, MPI_ELEM_T, MAIN_RANK, MPI_COMM_WORLD);

    if (rank == MAIN_RANK)
        free(input_buffer);

#ifdef DEBUG
    debug_1();
    MPI_Barrier(MPI_COMM_WORLD); /*must finish debug IO before continuing*/
#endif

    /*Phase 2 - split data into small buckets*/
    elem_t * small_buckets = (elem_t*)malloc(big_bucket_length * size *
                                             sizeof(elem_t));
    check_malloc(small_buckets,"small_buckets");
    size_t * small_bucket_lengths = (size_t*)calloc(size, sizeof(size_t));
    check_malloc(small_bucket_lengths, "small_bucket_lengths");

    split_small_buckets(small_bucket_lengths, small_buckets);

    free(big_bucket);

    /*Phase 3 - send each of small buckets to the processors*/
    MPI_Request * small_bucket_reqs =
        (MPI_Request*)malloc(2*(size-1)*sizeof(MPI_Request));
    check_malloc(small_bucket_reqs, "small_bucket_reqs");

    size_t small_bucket_reqs_ix = 0;

    elem_t * to_sort;
    size_t to_sort_length=0;
    size_t * incoming_lengths = (size_t*)calloc(size, sizeof(size_t));
    check_malloc(incoming_lengths, "incoming_lengths");

    incoming_lengths[rank] = small_bucket_lengths[rank];

    for (i=0; (int)i < size; i++){
        if ((int)i not_eq rank){ /*don't send to self*/
            MPI_Isend(small_bucket_lengths+i, 1, MPI_SIZE_T, i,
                      LENGTH_TAG, MPI_COMM_WORLD, small_bucket_reqs +
                      small_bucket_reqs_ix++);
            MPI_Irecv(incoming_lengths + i, 1, MPI_SIZE_T, i,
                      LENGTH_TAG, MPI_COMM_WORLD, small_bucket_reqs +
                      small_bucket_reqs_ix++);
        }
    }

    for (i=0; (int)i < 2 * (size-1); i++){
        MPI_Waitany(2 * (size-1), small_bucket_reqs, (int*)&j,
                    MPI_STATUS_IGNORE);
    }

    for (i=0; (int)i < size; i++)
        to_sort_length += incoming_lengths[i];

    to_sort = (elem_t*)malloc(to_sort_length * sizeof(elem_t));
    check_malloc(to_sort, "to_sort");

    /*index points to point in to_sort array into which to start recieving*/
    size_t recv_ix = 0;

    small_bucket_reqs_ix = 0;
    for (i=0; (int)i < size; i++){
        if ((int)i not_eq rank){
            MPI_Isend(small_buckets + i * big_bucket_length,
                      small_bucket_lengths[i], MPI_ELEM_T, i, DATA_TAG,
                      MPI_COMM_WORLD, small_bucket_reqs +
                      small_bucket_reqs_ix++);
            MPI_Irecv(to_sort + recv_ix, incoming_lengths[i],
                      MPI_ELEM_T, i, DATA_TAG, MPI_COMM_WORLD,
                      small_bucket_reqs + small_bucket_reqs_ix++);
        } else {
            memcpy(to_sort + recv_ix, small_buckets + i * big_bucket_length,
                   small_bucket_lengths[i]*sizeof(elem_t));
        }
        recv_ix += incoming_lengths[i];
    }

    free(incoming_lengths);

    for (i=0; (int)i < 2 * (size-1); i++){
        MPI_Waitany(2 * (size-1), small_bucket_reqs, (int*)&j,
                    MPI_STATUS_IGNORE);
    }

    free(small_buckets);
    free(small_bucket_lengths);
    free(small_bucket_reqs);

    /*Phase 4 - sort buckets*/

    qsort(to_sort, to_sort_length, sizeof(elem_t), gt);

#ifdef DEBUG
    debug_2(to_sort_length, to_sort);
    MPI_Barrier(MPI_COMM_WORLD);
#endif

#ifdef OUTPUT
    output_sorted(to_sort_length, to_sort);
#endif

    free(to_sort);

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();

    return EXIT_SUCCESS;
}

static inline void range(size_t len, elem_t* buf, elem_t extremes[2]){
    size_t i;

    elem_t max=INT_MIN;
    elem_t min=INT_MAX;

    for (i=0; i < len; i++){
        max = (buf[i] > max) ? buf[i] : max;
        min = (buf[i] < min) ? buf[i] : min;
    }
    extremes[0] = min;
    extremes[1] = max;
}

static inline void split_small_buckets(size_t* small_bucket_lengths,
                                       elem_t* small_buckets){
    size_t i, j;
    const ssize_t small_bucket_range =
        ((ssize_t)max_value - (ssize_t)min_value)/(ssize_t)size;

    for (i=0; i < big_bucket_length; i++){
        for (j=0; (int)j < size; j++){
            if ((ssize_t)big_bucket[i] <=
                ((ssize_t)min_value + ((ssize_t)j+1) * small_bucket_range)){
                small_buckets[j*big_bucket_length + small_bucket_lengths[j]++] =
                    big_bucket[i];
                break;
            } else if ((int)j == size-1){
                small_buckets[j*big_bucket_length + small_bucket_lengths[j]++] =
                    big_bucket[i];
                break;
            }
        }
    }
}

#ifdef OUTPUT
static inline void output_sorted(size_t to_sort_length, elem_t* to_sort){
    size_t i;

    if (rank == MAIN_RANK){
#ifdef DEBUG
        fprintf(stdout, "Sorted Result\n");
#endif

        for (i=0; i < to_sort_length; i++) print_elem_ln(stdout, to_sort[i]);
        fflush(stdout);

        if (size > 1)
            MPI_Send(&i, 1, MPI_INT, rank+1, SIGNAL_TAG, MPI_COMM_WORLD);
    } else if (rank == size -1){
        MPI_Recv(&i, 1, MPI_INT, rank-1, SIGNAL_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (i=0; i<to_sort_length; i++) print_elem_ln(stdout, to_sort[i]);
        fflush(stdout);

    } else {
        MPI_Recv(&i, 1, MPI_INT, rank-1, SIGNAL_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (i=0; i<to_sort_length; i++) print_elem_ln(stdout, to_sort[i]);
        fflush(stdout);

        MPI_Send(&i, 1, MPI_INT, rank+1, SIGNAL_TAG, MPI_COMM_WORLD);
    }
}
#endif

#ifdef DEBUG
static void debug_1(){
    size_t i;
    if (rank == MAIN_RANK){
        fprintf(stderr, "Process %d\n", rank);
        fprintf(stderr, "Min Value: "); print_elem_ln(stderr, min_value);
        fprintf(stderr, "Max Value: "); print_elem_ln(stderr, max_value);

        for (i=0;i<big_bucket_length;i++) print_elem_ln(stderr, big_bucket[i]);

        fflush(stderr);

        if (size > 1)
            MPI_Send(&i, 1, MPI_INT, rank+1, SIGNAL_TAG, MPI_COMM_WORLD);

    }else if (rank == size-1){
        MPI_Recv(&i, 1, MPI_INT, rank-1, SIGNAL_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        fprintf(stderr, "Process %d\n", rank);
        fprintf(stderr, "Min Value: "); print_elem_ln(stderr, min_value);
        fprintf(stderr, "Max Value: "); print_elem_ln(stderr, max_value);

        for (i=0;i<big_bucket_length;i++) print_elem_ln(stderr, big_bucket[i]);

        fflush(stderr);
    } else {
        MPI_Recv(&i, 1, MPI_INT, rank-1, SIGNAL_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        fprintf(stderr, "Process %d\n", rank);
        fprintf(stderr, "Min Value: "); print_elem_ln(stderr, min_value);
        fprintf(stderr, "Max Value: "); print_elem_ln(stderr, max_value);

        for (i=0;i<big_bucket_length;i++) print_elem_ln(stderr, big_bucket[i]);

        fflush(stderr);

        MPI_Send(&i, 1, MPI_INT, rank+1, SIGNAL_TAG, MPI_COMM_WORLD);
    }
}

static void debug_2(size_t to_sort_length, elem_t* to_sort){
    size_t i;
    if (rank == MAIN_RANK){
        fprintf(stderr, "After partial sorting into buckets\n");
        fprintf(stderr, "Process %d\n", rank);
        for (i=0; i<to_sort_length; i++) print_elem_ln(stderr, to_sort[i]);

        fflush(stderr);

        if (size > 1)
            MPI_Send(&i, 1, MPI_INT, rank+1, SIGNAL_TAG, MPI_COMM_WORLD);
    }else if (rank == size-1){
        MPI_Recv(&i, 1, MPI_INT, rank-1, SIGNAL_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        fprintf(stderr, "Process %d\n", rank);
        for (i=0;i<to_sort_length;i++) print_elem_ln(stderr, to_sort[i]);
        fflush(stderr);
    } else {
        MPI_Recv(&i, 1, MPI_INT, rank-1, SIGNAL_TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        fprintf(stderr, "Process %d\n", rank);
        for (i=0; i<to_sort_length; i++) print_elem_ln(stderr, to_sort[i]);

        fflush(stderr);

        MPI_Send(&i, 1, MPI_INT, rank+1, SIGNAL_TAG, MPI_COMM_WORLD);
    }
}
#endif
