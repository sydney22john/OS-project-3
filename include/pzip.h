#ifndef PZIP_H
#define PZIP_H

#include <stdint.h>

/**
 * The struct that holds consecutive character-occurence pairs.
 */
struct zipped_char {
	char character;
	uint8_t occurence;
};

struct write_to_location {
	int length;
	int start_index;
};

struct thread_args {
	int n_threads;
	char *input_chars;
	int input_chars_size;
	struct zipped_char *zipped_chars;
	int *zipped_chars_count;
	int *char_frequency;
	int order;
};

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
	  int *char_frequency);


void thread_execution(int n_threads, char *input_chars, int input_chars_size, struct zipped_char *zipped_chars, int *zipped_chars_count, int *char_frequency, int order);
void create_threads(int n_threads, char *input_chars, int input_chars_size, struct zipped_char *zipped_chars, int *zipped_chars_count, int *char_frequency);
void append_to_zipped_chars(char character, int occurence, struct zipped_char **zipped_chars, int index);
struct zipped_char **compress(char *input_chars, int input_chars_size, int *char_frequency, int order);
void increment_char_frequency(int *char_frequency, char character, int occurences);
struct write_to_location *malloc_lengths(int n_threads);
struct thread_args *create_args(int n_threads, char *input_chars, int input_chars_size, struct zipped_char *zipped_chars, int *zipped_chars_count, int *char_frequency, int index);
int calc_starting_index(int order);
void write_to_zipped_chars(struct zipped_char *global_zipped_chars, struct zipped_char **local_zipped_chars, int order);
void free_zipped_chars_subsets(struct zipped_char **zipped_chars, int size);
void free_args(struct thread_args *args[], int size);
void *arg_unpacking(void *args);

#endif /* PZIP_H */
