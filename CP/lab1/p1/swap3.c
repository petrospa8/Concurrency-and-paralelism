#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "options.h"

struct buffer {
	int *data;
	int size;
	pthread_mutex_t  *mutex;
};

struct thread_info {
	pthread_t       thread_id;        // id returned by pthread_create()
	int             thread_num;       // application defined thread #
};

struct args {
	int 		thread_num;       // application defined thread #
	int 	        delay;			  // delay between operations
	struct buffer   *buffer;		  // Shared buffer
	int *iterations;
	pthread_mutex_t  iterMutex;
};


void *swap(void *ptr)
{
	int i,j, tmp;
	struct args *args =  ptr;
	while(1) {
		pthread_mutex_lock(&args->iterMutex);
		if(args->iterations > 0){
			args->iterations --;
			pthread_mutex_unlock(&args->iterMutex);
			i = rand() % args->buffer->size;

			do	{j = rand() % args->buffer->size;}
				while(i == j);

			if (i < j) {
				pthread_mutex_lock(&args->buffer->mutex[i]);
				pthread_mutex_lock(&args->buffer->mutex[j]);
			}
			else{
				pthread_mutex_lock(&args->buffer->mutex[j]);
				pthread_mutex_lock(&args->buffer->mutex[i]);
			}
			printf("Thread %d swapping positions %d (== %d) and %d (== %d)\n",
				args->thread_num, i, args->buffer->data[i], j, args->buffer->data[j]);

			tmp = args->buffer->data[i];
			if(args->delay) usleep(args->delay); // Force a context switch

			args->buffer->data[i] = args->buffer->data[j];
			if(args->delay) usleep(args->delay);

			args->buffer->data[j] = tmp;
			if(args->delay) usleep(args->delay);

			pthread_mutex_unlock(&args->buffer->mutex[i]);
			pthread_mutex_unlock(&args->buffer->mutex[j]);
		}
		else{
			pthread_mutex_unlock(&args->iterMutex);
			break;
		}
	}
	return NULL;
}

void print_buffer(struct buffer buffer) {
	int i;

	for (i = 0; i < buffer.size; i++)
		printf("%i ", buffer.data[i]);
	printf("\n");
}

void start_threads(struct options opt)
{
	int i,j;
	pthread_mutex_t iterLock, mutex_arr[opt.buffer_size];
	struct thread_info *threads;
	struct args *args;
	struct buffer buffer;
	//int *remaining = &opt.iterations;
	srand(time(NULL));

	if((buffer.data=malloc(opt.buffer_size*sizeof(int)))==NULL) {
		printf("Out of memory\n");
		exit(1);
	}
	buffer.size = opt.buffer_size;

	pthread_mutex_init(&iterLock, NULL);

	for(i=0; i<buffer.size; i++){
		pthread_mutex_init(&mutex_arr[i], NULL);
		buffer.data[i]=i;
	}

	printf("creating %d threads\n", opt.num_threads);
	threads = malloc(sizeof(struct thread_info) * opt.num_threads);

	args = malloc(sizeof(struct args) * opt.num_threads);
	args->iterations = malloc(sizeof(int));
	args->iterations[0] = opt.iterations;
	if (threads == NULL || args==NULL) {
		printf("Not enough memory\n");
		exit(1);
	}

	printf("Buffer before: ");
	print_buffer(buffer);


	// Create num_thread threads running swap()
	for (i = 0; i < opt.num_threads; i++) {
		threads[i].thread_num = i;

		args[i].thread_num = i;
		args[i].buffer     = &buffer;
		args[i].delay      = opt.delay;
		args[i].buffer->mutex = mutex_arr;
		args[i].iterMutex = iterLock;

		if ( 0 != pthread_create(&threads[i].thread_id, NULL,
					 swap, &args[i])) {
			printf("Could not create thread #%d", i);
			exit(1);
		}
	}

	// Wait for the threads to finish
	for (i = 0; i < opt.num_threads; i++){
		pthread_join(threads[i].thread_id, NULL);
		for(j = 0; j < args[i].buffer->size; j++){
			pthread_mutex_destroy(&args[i].buffer->mutex[j]);
		}
	}
	// Print the buffer
	printf("Buffer after:  ");
	print_buffer(buffer);

	free(args);
	free(threads);
	free(buffer.data);

	pthread_exit(NULL);
}

int main (int argc, char **argv)
{
	struct options opt;

	// Default values for the options
	opt.num_threads = 10;
	opt.buffer_size = 10;
	opt.iterations  = 100;
	opt.delay       = 10;

	read_options(argc, argv, &opt);

	start_threads(opt);

	exit (0);
}
