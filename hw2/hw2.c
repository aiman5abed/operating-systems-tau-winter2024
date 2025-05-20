#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_COUNTERS 100
#define MAX_THREADS 4096
#define QUEUE_CAPACITY 100

// Structure to represent a command
typedef struct {
    char command[MAX_COMMAND_LENGTH];
    long long start_time; // Start time of the job
    long long end_time;   // End time of the job
} Command;

// Work queue structure
typedef struct {
    Command* commands[QUEUE_CAPACITY];
    int front;
    int rear;
    int size;
    int capacity;
    int done;
    int counter_jobs;
    pthread_mutex_t mutex;
    pthread_cond_t cond_empty;
    pthread_cond_t cond_full;
    pthread_cond_t cond_wait;
} WorkQueue;

// Global variables
WorkQueue work_queue;
pthread_t worker_threads[MAX_THREADS];
int num_threads;
int log_enabled;
long long total_running_time = 0;
long long sum_turnaround_time = 0;
long long min_turnaround_time = -1;
long long max_turnaround_time = 0;

// Struct thread data
typedef struct {
    long long start_time;
    int thread_num;

} thread_data;


// Function prototypes
void* worker_thread(void* arg);
void initialize_work_queue();
void enqueue_work(Command* command);
Command* dequeue_work();
void parse_dispatcher_command(char* line);
void parse_worker_job(char* line,long long reading_line_time);
void msleep(int milliseconds);
void increment_counter(int counter_id);
void decrement_counter(int counter_id);
void repeat_commands(char* line, int times,int thread_id);
long long get_current_time();
void write_to_log(const char* filename, const char* format, ...);
void calculate_statistics();

void create_counter_files(int num_counters);
void* worker_thread(void* arg);

int main(int argc, char *argv[]) {
    long long start_time = get_current_time(); // Record start time
    long long reading_line_time;
    if (argc != 5) {
        printf("Usage: %s cmdfile.txt num_threads num_counters log_enabled\n", argv[0]);
        return 1;
    }

    // Parse command line arguments
    char *cmdfile = argv[1];
    num_threads = atoi(argv[2]);
    int num_counters = atoi(argv[3]);
    log_enabled = atoi(argv[4]);

    // Initialize work queue
    initialize_work_queue(); 
    // Create counter files
    create_counter_files(num_counters);
    thread_data threads_data_arr [num_threads];

    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        threads_data_arr[i].start_time=start_time;
        threads_data_arr[i].thread_num=i;
        pthread_create(&worker_threads[i], NULL, worker_thread, (void*) &threads_data_arr[i]);
    }

    // Open dispatcher log file
    FILE* dispatcher_log = NULL;
    if (log_enabled) {
        dispatcher_log = fopen("dispatcher.txt", "w");
        if (dispatcher_log == NULL) {
            perror("Error opening dispatcher log file");
            return 1;
        }
    }

    // Read commands from file and enqueue them
    FILE *file = fopen(cmdfile, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    char line[MAX_COMMAND_LENGTH];
    char *ptr;
    while (fgets(line, sizeof(line), file)) {
        ptr = line;
        while (*ptr==' ')
            ptr++;
        reading_line_time=get_current_time();
        // Trim newline character if present
        ptr[strcspn(ptr, "\n")] = '\0';

        // Log the command
        if (log_enabled) {
            long long current_time = get_current_time();
            fprintf(dispatcher_log, "TIME %lld: read cmd line: %s\n", current_time-start_time, ptr);
        }

        if (strncmp(ptr, "dispatcher", 10) == 0) {
            parse_dispatcher_command(ptr);
        } else if (strncmp(ptr, "worker", 6) == 0) {
            parse_worker_job(ptr,reading_line_time);
        }
    }
    fclose(file);
    work_queue.done=1;
    pthread_cond_signal(&work_queue.cond_empty);
    pthread_mutex_unlock(&work_queue.mutex);

    // Wait for all pending background commands to complete
    for (int i=0;i<num_threads;i++)
    {
        pthread_join(worker_threads[i],NULL);
    }
    long long end_time = get_current_time();
    total_running_time = end_time - start_time;


    // Write statistics to stats.txt
    FILE* stats_file = fopen("stats.txt", "w");
    if (stats_file == NULL) {
        perror("Error opening stats file");
        return 1;
    }
    fprintf(stats_file, "total running time: %lld milliseconds\n", total_running_time);
    fprintf(stats_file, "sum of jobs turnaround time: %lld milliseconds\n", sum_turnaround_time);
    fprintf(stats_file, "min job turnaround time: %lld milliseconds\n", min_turnaround_time);
    fprintf(stats_file, "average job turnaround time: %f milliseconds\n", (double)sum_turnaround_time / (double)(work_queue.counter_jobs));
    fprintf(stats_file, "max job turnaround time: %lld milliseconds\n", max_turnaround_time);
    fclose(stats_file);


    return 0;
}

// Worker thread function
void* worker_thread(void* arg) {
    char copy_command [MAX_COMMAND_LENGTH];
    thread_data  *data= (thread_data *) arg;
    int thread_id = data->thread_num;
    long long start_time = data->start_time;
    long long turnaround_time;
    long long current_time;
    Command *command;
    char log_filename[20];
    snprintf(log_filename, sizeof(log_filename), "thread%d.txt", thread_id);
    FILE* thread_log = NULL;
    if (log_enabled) {
        thread_log = fopen(log_filename, "w");
        if (thread_log == NULL) {
            perror("Error opening thread log file");
            return NULL;
        }
    }

    while (1) {
        // Dequeue work from the queue
        command = dequeue_work();
        // Check if there's no more work
        if (command==NULL) {
            break;
        }

        // Log the start of the job
        if (log_enabled) {
            current_time=get_current_time();
            fprintf(thread_log, "TIME %lld: START job %s\n", current_time-start_time, command->command);
        }


        // Tokenize the command line
        char* token;
        strcpy(copy_command,command->command);
        char* rest = copy_command;
        while ((token = strtok_r(rest, ";", &rest))) {
            // Trim leading and trailing whitespaces
            char* trimmed_token = strtok(token, " ");
            if (trimmed_token == NULL) {
                continue;
            }

            if (strcmp(trimmed_token, "msleep") == 0) {
                int milliseconds;
                if ((token = strtok(NULL, " ")) != NULL) {
                    milliseconds = atoi(token);
                    msleep(milliseconds);
                }
            } else if (strcmp(trimmed_token, "increment") == 0) {
                int counter_id;
                if ((token = strtok(NULL, " ")) != NULL) {
                    counter_id = atoi(token);
                    increment_counter(counter_id);
                }
            } else if (strcmp(trimmed_token, "decrement") == 0) {
                int counter_id;
                if ((token = strtok(NULL, " ")) != NULL) {
                    counter_id = atoi(token);
                    decrement_counter(counter_id);
                }
            } else if (strcmp(trimmed_token, "repeat") == 0) {
                int times;
                if ((token = strtok(NULL, " ")) != NULL) {
                    times = atoi(token);
                    repeat_commands(rest, times,thread_id);
                    break; // Stop processing after encountering a repeat command
                }
            }
        }
                // Update statistics
        pthread_mutex_lock(&work_queue.mutex);
        command->end_time = get_current_time();
        turnaround_time = command->end_time - command->start_time;
        sum_turnaround_time += turnaround_time;
        if ((turnaround_time < min_turnaround_time) || min_turnaround_time == -1) {
            min_turnaround_time = turnaround_time;
        }
        if (turnaround_time > max_turnaround_time) {
            max_turnaround_time = turnaround_time;
        }
        pthread_mutex_unlock(&work_queue.mutex);

             // Log the end of the job
        if (log_enabled) {
            current_time=get_current_time();
            fprintf(thread_log, "TIME %lld: END job %s\n", current_time-start_time, command->command);
        }
        //free space of command 
        free(command);

    }

    // Close thread log file
    if (log_enabled) {
        fclose(thread_log);
    }
    pthread_exit(NULL);
}

//Create counter files
void create_counter_files(int num_counters) {
    for (int i = 0; i < num_counters; i++) {
        char filename[20];
        snprintf(filename, sizeof(filename), "count%02d.txt", i);
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
            perror("Error creating counter file");
            exit(EXIT_FAILURE);
        }
        fprintf(file, "0\n"); // Initialize counter value to 0
        fclose(file);
    }
}

// Initialize work queue
void initialize_work_queue() {
    work_queue.capacity= QUEUE_CAPACITY;
    work_queue.size = 0;
    work_queue.front = 0;
    work_queue.rear = -1;
    work_queue.done=0;
    work_queue.counter_jobs=0;
    pthread_mutex_init(&work_queue.mutex, NULL);
    pthread_cond_init(&work_queue.cond_empty, NULL);
    pthread_cond_init(&work_queue.cond_full, NULL);
}

// Enqueue work into the queue
void enqueue_work(Command *command) {
    pthread_mutex_lock(&work_queue.mutex);
    while (work_queue.size >= work_queue.capacity) {
        pthread_cond_wait(&work_queue.cond_full, &work_queue.mutex);
    }
    work_queue.rear = (work_queue.rear + 1) % work_queue.capacity;
    work_queue.commands[work_queue.rear] = command;
    work_queue.size++;
    work_queue.counter_jobs++;
    pthread_cond_signal(&work_queue.cond_empty);
    pthread_mutex_unlock(&work_queue.mutex);
}

// Dequeue work from the queue
Command* dequeue_work() {
    Command *command;
    command=NULL;

    pthread_mutex_lock(&work_queue.mutex);
    while (work_queue.size <= 0 && !work_queue.done ) {
        pthread_cond_broadcast(&work_queue.cond_wait);
        pthread_cond_wait(&work_queue.cond_empty, &work_queue.mutex);

    }
    if (work_queue.size>0)
    {
        command = work_queue.commands[work_queue.front];
        work_queue.front = (work_queue.front + 1) % work_queue.capacity;
        work_queue.size--;
        pthread_cond_signal(&work_queue.cond_full);
        pthread_mutex_unlock(&work_queue.mutex);

    }
    else
    // no more work, wake up stuck threads
    {
        pthread_cond_signal(&work_queue.cond_empty);
        pthread_mutex_unlock(&work_queue.mutex);

    }

    return command;
}

// Parse and execute a dispatcher command
void parse_dispatcher_command(char* line) {
    char command[MAX_COMMAND_LENGTH];
    int arg=-1;

    sscanf(line, "%*[^_]_%s %d", command, &arg);
    

    if (strcmp(command, "msleep") == 0) {
        if (arg >0)
        {
            usleep(arg * 1000);
        }
        else 
            printf("invalid command\n");
    } else if (strcmp(command, "wait") == 0) {
        pthread_mutex_lock(&work_queue.mutex);
        while (work_queue.size > 0 ) {
            pthread_cond_wait(&work_queue.cond_wait, &work_queue.mutex);
        }
        pthread_mutex_unlock(&work_queue.mutex);
        
    } 

    
}

// Parse and enqueue a worker job
void parse_worker_job(char* line,long long reading_line_time) {
    char job[MAX_COMMAND_LENGTH];
    strcpy(job, line + 7); // Skip "worker" prefix

    // Enqueue the worker job
    Command *command=(Command *)malloc(sizeof(Command));
    command->start_time=reading_line_time;
    strcpy(command->command, job);
    enqueue_work(command);
}

// Sleep for the specified number of milliseconds
void msleep(int milliseconds) {
    usleep(milliseconds * 1000);
}

// Increment the counter value in the specified counter file
void increment_counter(int counter_id) {
    char filename[20];
    snprintf(filename, sizeof(filename), "count%02d.txt", counter_id);
    FILE *file = fopen(filename, "r+");
    if (file == NULL) {
        printf("Error opening counter file %s for incrementing: %s\n", filename, strerror(errno));
        return;
    }
    long long value;
    fscanf(file, "%lld", &value);
    value++;
    fseek(file, 0, SEEK_SET);
    fprintf(file, "%lld", value);
    fclose(file);
}

// Decrement the counter value in the specified counter file
void decrement_counter(int counter_id) {
    char filename[20];
    snprintf(filename, sizeof(filename), "count%02d.txt", counter_id);
    FILE *file = fopen(filename, "r+");
    if (file == NULL) {
        printf("Error opening counter file %s for decrementing: %s\n", filename, strerror(errno));
        return;
    }
    long long value;
    fscanf(file, "%lld", &value);
    value--;
    fseek(file, 0, SEEK_SET);
    fprintf(file, "%lld", value);
    fclose(file);
}

// Repeat the specified commands for the specified number of times
void repeat_commands(char* line, int times,int thread_id) {
    char copy_line [MAX_COMMAND_LENGTH];
    for (int i = 0; i < times; i++) {
        char* token;
        strcpy(copy_line,line);
        char* rest = copy_line;
        while ((token = strtok_r(rest, ";",&rest))) {
            // Trim leading and trailing whitespaces
            char* trimmed_token = strtok(token, " ");
            if (trimmed_token == NULL) {
                continue;
            }

            if (strcmp(trimmed_token, "msleep") == 0) {
                int milliseconds;
                if ((token = strtok(NULL, " ")) != NULL) {
                    milliseconds = atoi(token);
                    msleep(milliseconds);
                }
            } else if (strcmp(trimmed_token, "increment") == 0) {
                int counter_id;
                if ((token = strtok(NULL, " ")) != NULL) {
                    counter_id = atoi(token);
                    increment_counter(counter_id);
                }
            } else if (strcmp(trimmed_token, "decrement") == 0) {
                int counter_id;
                if ((token = strtok(NULL, " ")) != NULL) {
                    counter_id = atoi(token);
                    decrement_counter(counter_id);
                }
            } 
            }
        }

    }
    
// Get the current time in milliseconds
long long get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (long long)(tv.tv_usec) / 1000;
}
