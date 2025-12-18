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

// Fungsi child_process_work() merepresentasikan kerja suatu process.
// Nanti diganti pakai scheduler -> berdasarkan burst time.
void child_process_work(int id) {
    while (1) {
        printf("[Process P%d] Executing...\n", id);
        sleep(1);   // Simulasi 1 detik eksekusi CPU
    }
}

// Function untuk buat child process pakai system call fork().
pid_t create_child_process(int process_id) {
    pid_t retval;       // Simpan return value dari fork().
    retval = fork();    // Buat child process baru.

    // Jika retval 0, child process berhasil dibentuk.
    // Kondisi ini yang akan dijalankan oleh child process.
    if (retval == 0) {
        raise(SIGSTOP);          // Child process distop dulu supaya tidak langsung jalan, harus tunggu scheduler.

        child_process_work(process_id);  // Simulasi kerja process pakai function child_process_work().
        exit(0);                 // Child process diterminate saat child_process_work() selesai.
    } else if (retval == -1){
        // Jika retval -1, fork() gagal dan child process tidak dapat dibuat.
        return -1;
    }

    return retval;   // Return PID child processnya.
}

// Function untuk membuat satu PCB suatu process.
PCB create_pcb(int id, int arrival_time, int burst_time) {
    PCB pcb;

    pcb.id = id;        // Set id dari parameter.
    pcb.pid = -1;       // PID belum ada (belum difork).
    pcb.state = READY;  // State awal process.

    // Set informasinya berdasarkan parameter.
    pcb.arrival_time = arrival_time;
    pcb.burst_time = burst_time;
    pcb.remaining_time = burst_time;

    // Set default information timenya.
    pcb.waiting_time = 0;
    pcb.turnaround_time = 0;
    pcb.finish_time = -1;

    return pcb; // Return pcb yang sudah diisi.
}

// Parameter untuk Round Robin Scheduler.
#define MAX_PROCESS 5
#define TIME_QUANTUM 2

int main() {
    printf("Round Robin Scheduler Setup\n");
    printf("Time Quantum: %d\n", TIME_QUANTUM);

    // Pakai struct PCB yang sudah dibentuk.
    PCB pcb[MAX_PROCESS];
    // Buat inisialisasi 5 process.
    // Parameter yang dikirim -> id, arrival time, burst time.
    pcb[0] = create_pcb(1, 0, 5);
    pcb[1] = create_pcb(2, 1, 3);
    pcb[2] = create_pcb(3, 2, 1);
    pcb[3] = create_pcb(4, 3, 2);
    pcb[4] = create_pcb(5, 4, 3);

    printf("Creating processes...\n");

    // Buat child process menggunakan create_child_process().
    for (int i = 0; i < MAX_PROCESS; i++) {
        pid_t pid = create_child_process(pcb[i].id);

        if (pid > 0) {
            // Simpan PID child ke PCB.
            pcb[i].pid = pid;
            pcb[i].state = READY;

            printf("P%d created and registered with PID %d\n", pcb[i].id, pid);
        } 
        else {
            // Fork gagal.
            printf("Failed to fork process P%d\n", pcb[i].id);
            exit(1);
        }
    }

    // Scheduler belum dijalankan
    while (1) {
        pause();
    }

    return 0;
}