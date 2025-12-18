#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>

// Process state sebagai representasi status execution suatu process dalam OS.
typedef enum {
    READY,          // Process ready dan sedang menunggu untuk dieksekusi.
    RUNNING,        // Process sedang run atau dieksekusi.
    FINISHED        // Process selesai dieksekusi.
} ProcessState;

// Process Control Blocks untuk simpan informasi sebuah process 
// yang dibutuhkan oleh scheduler dalam mengatur process execution.
typedef struct {
    int id;                 // Process ID -> P1, P2, P3.
    pid_t pid;              // PID child process hasil fork().
    ProcessState state; 

    // Informasi waktu untuk suatu process.
    int arrival_time;       // Waktu kedatangan process di queue.
    int burst_time;         // Total execution time suatu process.
    int remaining_time;     // Sisa waktu execution yang masih dibutuhkan process.
    int finish_time;        // Waktu selesai execution suatu process.

    int waiting_time;       // Total waiting time process di queue.
    int turnaround_time;    // Turnaround Time = finish - arrival.
} PCB;
