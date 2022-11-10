#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pzip.h"

// Globals
pthread_mutex_t char_frequency_access = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t zipped_chars_access = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lengths_access = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;
struct write_to_location *lengths;

/**
 * pzip() - zip an array of characters in parallel
 *
 * Inputs:
 * @n_threads:		   The number of threads to use in pzip
 * @input_chars:		   The input characters (a-z) to be zipped
 * @input_chars_size:	   The number of characaters in the input file
 *
 * Outputs:
 * @zipped_chars:       The array of zipped_char structs
 * @zipped_chars_count:   The total count of inserted elements into the zippedChars array.
 * @char_frequency[26]: Total number of occurences
 *
 * NOTE: All outputs are already allocated. DO NOT MALLOC or REASSIGN THEM !!!
 *
 */
void pzip(int n_threads, char *input_chars, int input_chars_size,
	  struct zipped_char *zipped_chars, int *zipped_chars_count,
	  int *char_frequency)
{
	create_threads(n_threads, input_chars, input_chars_size, zipped_chars, zipped_chars_count, char_frequency);
}

// add to char_frequency when we are appending local zipped_chars to global zipped_chars
void create_threads(int n_threads, char *input_chars, int input_chars_size,
	struct zipped_char *zipped_chars, int *zipped_chars_count, int *char_frequency) 
{
	// array of threads
	pthread_t threads[n_threads];
	struct thread_args *thread_args[n_threads];
	// initalizing the pthread_barrier
	pthread_barrier_init(&barrier, NULL, n_threads + 1);
	// initalizing global lengths array
	lengths = malloc_lengths(n_threads);

	// creating the threads
	for(int i = 0; i < n_threads; i++) {
		thread_args[i] = create_args(n_threads, input_chars, input_chars_size, zipped_chars, zipped_chars_count, char_frequency, i);
		pthread_create(&threads[i], NULL, arg_unpacking, (void *)thread_args[i]);
	}

	pthread_barrier_wait(&barrier);

	// joining the threads
	for (int i = 0; i < n_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	*zipped_chars_count = lengths[n_threads - 1].length + lengths[n_threads - 1].start_index;

	free(lengths);
	free_args(thread_args, n_threads);
}

void *arg_unpacking(void *args) {
	struct thread_args *t_args = (struct thread_args*) args;
	thread_execution(t_args->n_threads, t_args->input_chars, t_args->input_chars_size,
		t_args->zipped_chars, t_args->zipped_chars_count, t_args->char_frequency, t_args->order);
	return NULL;
}

void thread_execution(int n_threads, char *input_chars, int input_chars_size, struct zipped_char *zipped_chars, int *zipped_chars_count, int *char_frequency, int order) {
	struct zipped_char **zipped_chars_local = compress(input_chars, input_chars_size, char_frequency, order);
	// wait until all the threads have created their zipped characters
	pthread_barrier_wait(&barrier);

	write_to_zipped_chars(zipped_chars, zipped_chars_local, order);

	free_zipped_chars_subsets(zipped_chars_local, lengths[order].length);
}

void write_to_zipped_chars(struct zipped_char *global_zipped_chars, struct zipped_char **local_zipped_chars, int order) {
	// determine starting index for the global zipped chars
	pthread_mutex_lock(&lengths_access);
	if(lengths[order].start_index == -1) {
		calc_starting_index(order);
	}
	int starting_index = lengths[order].start_index;
	pthread_mutex_unlock(&lengths_access);

	// copying local result to global result in parallel
	// printf("BEFORE SEG FAULT - GLOBAL COPYING\n");
	for(int i = 0; i < lengths[order].length; i++) {
		global_zipped_chars[i + starting_index].character = local_zipped_chars[i]->character;
		global_zipped_chars[i + starting_index].occurence = local_zipped_chars[i]->occurence;
	}
	// printf("AFTER SEG FAULT - GLOBAL COPYING\n");
}

int calc_starting_index(int order) {
	// base case #1
	if(order == 0 || lengths[order].start_index != -1) return lengths[order].length + lengths[order].start_index;

	lengths[order].start_index = calc_starting_index(order - 1);
	return lengths[order].start_index + lengths[order].length;
}

struct thread_args *create_args(int n_threads, char *input_chars, int input_chars_size, 
		struct zipped_char *zipped_chars, int *zipped_chars_count, int *char_frequency, int index) 
{
	struct thread_args *arg;
	if ( (arg = malloc(sizeof(struct thread_args))) == NULL) {
		fprintf(stderr, "Error: malloc failed\n");
		exit(EXIT_FAILURE);
	}
	int size = input_chars_size / n_threads;

	arg->n_threads = n_threads;
	arg->input_chars = input_chars + index * size;
	arg->input_chars_size = size;
	arg->zipped_chars = zipped_chars;
	arg->zipped_chars_count = zipped_chars_count;
	arg->char_frequency = char_frequency;
	arg->order = index;
	return arg;
}

// I think compress should return a pointer to a zipped_char struct and then at the end of the %rename% create_threads rountine
// once they have all completed we go in succession adding the local results to the global array
struct zipped_char **compress(char *input_chars, int input_chars_size, int *char_frequency, int order) {
	struct zipped_char **zipped_chars;
	if ( (zipped_chars = malloc(sizeof(struct zipped_char *) * input_chars_size)) == NULL ) {
		fprintf(stderr, "Error: malloc failed\n");
		exit(EXIT_FAILURE);
	}

	int char_ptr = 0, runner = 0;
	int zipped_chars_index = 0;
	while(true) {
		if(runner == input_chars_size || input_chars[char_ptr] != input_chars[runner]) {
			int occurences = runner - char_ptr;
			append_to_zipped_chars(input_chars[char_ptr], occurences, zipped_chars, zipped_chars_index++);
			char_ptr = runner;

			// writing to the frequency table
			pthread_mutex_lock(&char_frequency_access);
			increment_char_frequency(char_frequency, input_chars[char_ptr], occurences);
			pthread_mutex_unlock(&char_frequency_access);
		}
		if(runner == input_chars_size) break;
		runner++;
	}

	lengths[order].length = zipped_chars_index;

	return zipped_chars;
}

void increment_char_frequency(int *char_frequency, char character, int occurences) {
	char_frequency[character - 'a'] += occurences;
}

void append_to_zipped_chars(char character, int occurence, struct zipped_char **zipped_chars, int index) {
	struct zipped_char *temp;
	if ( (temp = malloc(sizeof(struct zipped_char))) == NULL) {
		fprintf(stderr, "Error: malloc failed\n");
		exit(EXIT_FAILURE);
	}

	temp->character = character;
	temp->occurence = occurence;
	zipped_chars[index] = temp;
}


// TODO: free thread_args function
void free_args(struct thread_args *args[], int size) {
	for(int i = 0; i < size; i++) {
		free(args[i]);
	}
}
// TODO: free the allocated zipped_char array
void free_zipped_chars_subsets(struct zipped_char **zipped_chars, int size) {
	for(int i = 0; i < size; i++) {
		free(zipped_chars[i]);
	}
	free(zipped_chars);
}

struct write_to_location *malloc_lengths(int n_threads) {
	struct write_to_location *temp;
	if( (temp = malloc(sizeof(struct write_to_location) * n_threads)) == NULL) {
		fprintf(stderr, "Error: malloc failed\n");
		exit(-1);
	}
	// base case for the dynamic programming approach
	temp[0].start_index = 0;
	// initalize all the starting indices to -1 besides the first position
	for(int i = 1; i < n_threads; i++) {
		temp[i].start_index = -1;
	}
	return temp;
}
