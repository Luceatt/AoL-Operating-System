#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

// GLOBAL VARIABLE:
// Parameter untuk Round Robin Scheduler.
#define MAX_PROCESS 5
#define TIME_QUANTUM 2

int current_time = 0;       // Waktu simulasi sistem (dalam detik).
int current_process = -1;   // Index process yang sedang RUNNING.
volatile sig_atomic_t quantum_expired = 0;  // Flag untuk tandain bahwa time quantum telah habis.

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

// Signal handler untuk SIGALRM (timer interrupt)
void timer_handler(int signum) {
    // Hindari warning unused parameter
    (void)signum;

    // Tandai bahwa quantum telah habis
    quantum_expired = 1;
}

// Function untuk mengaktifkan timer quantum
void setup_timer() {
    struct itimerval timer;

    // Waktu sampai SIGALRM pertama
    timer.it_value.tv_sec = TIME_QUANTUM;
    timer.it_value.tv_usec = 0;

    // Interval SIGALRM berikutnya
    timer.it_interval.tv_sec = TIME_QUANTUM;
    timer.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &timer, NULL);
}

// Untuk stop timer saat semua proses dalam state FINISHED.
void stop_timer() {
    struct itimerval timer;

    // Set semua nilai timer ke 0 → timer berhenti
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &timer, NULL);
}

// Function untuk mendaftarkan signal handler
void setup_signal_handler() {
    struct sigaction sa;

    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGALRM, &sa, NULL);
}

// ROUND ROBIN Scheduler Function
int select_next_process(PCB pcb[], int n) {
    int start = (current_process + 1) % n;    // Menentukan indeks awal pencarian (round-robin)

    for (int i = 0; i < n; i++) {    // Loop maksimal sebanyak jumlah proses
        int idx = (start + i) % n;    // Hitung indeks proses secara melingkar

        if (pcb[idx].state == READY &&      // Proses dalam keadaan READY
            pcb[idx].remaining_time > 0 &&     // Masih punya waktu eksekusi
            pcb[idx].arrival_time <= current_time) {    // Sudah tiba di waktu sekarang    
            return idx;    // Kembalikan indeks proses yang dipilih
        }
    }
    return -1;     // Tidak ada proses yang siap dijalankan
}

void update_waiting_time(PCB pcb[], int n) {
    for (int i = 0; i < n; i++) {    // Loop semua proses
        if (pcb[i].state == READY &&     // Proses dalam keadaan READY
            pcb[i].arrival_time <= current_time &&    // Proses sudah tiba
            i != current_process) {    // Bukan proses yang sedang berjalan
            pcb[i].waiting_time += TIME_QUANTUM;    // Tambah waktu tunggu sesuai time quantum
        }
    }
}

void finish_process(PCB pcb[], int index) {
    pcb[index].state = FINISHED;    // Ubah status proses menjadi FINISHED
    pcb[index].finish_time = current_time;    // Simpan waktu selesai proses

    pcb[index].turnaround_time =
        pcb[index].finish_time - pcb[index].arrival_time;    // Hitung turnaround time

    pcb[index].waiting_time =
        pcb[index].turnaround_time - pcb[index].burst_time;    // Hitung waiting time akhir
}

void round_robin_scheduler(PCB pcb[], int n) {

    // Jika ada process RUNNING → preempt
    if (current_process != -1) {
        kill(pcb[current_process].pid, SIGSTOP);    // Hentikan sementara proses yang sedang berjalan

        pcb[current_process].remaining_time -= TIME_QUANTUM;    // Kurangi sisa waktu eksekusi

        // Update waiting time proses lain
        update_waiting_time(pcb, n);    

        current_time += TIME_QUANTUM;    // Tambahkan waktu global sesuai time quantum

        // Jika selesai
        if (pcb[current_process].remaining_time <= 0) {
            finish_process(pcb, current_process);     // Tandai proses selesai
        } else {
            pcb[current_process].state = READY;    // Kembalikan ke state READY
        }
    }

    // Pilih process berikutnya
    int next = select_next_process(pcb, n);

    if (next == -1) {
        return; // Tidak ada process READY saat ini
    }

    current_process = next;    // Set proses terpilih sebagai current process
    pcb[current_process].state = RUNNING;    // Ubah state menjadi RUNNING
    kill(pcb[current_process].pid, SIGCONT);    // Lanjutkan eksekusi proses

    quantum_expired = 0;    // Reset penanda time quantum
}

void display_process_status(PCB pcb[], int n) {
    printf("\n[TIME %d]\n", current_time);
    for (int i = 0; i < n; i++) {
        printf("P%d | State: %d | Remaining: %d | Waiting: %d\n",
               pcb[i].id,
               pcb[i].state,
               pcb[i].remaining_time,
               pcb[i].waiting_time);
    }
}

void print_final_result(PCB pcb[], int n) {
    int total_wt = 0, total_tat = 0;

    printf("\nFinal Result:\n");
    printf("PID | AT | BT | WT | TAT\n");

    for (int i = 0; i < n; i++) {
        printf("P%d  | %d  | %d  | %d  | %d\n",
               pcb[i].id,
               pcb[i].arrival_time,
               pcb[i].burst_time,
               pcb[i].waiting_time,
               pcb[i].turnaround_time);

        total_wt += pcb[i].waiting_time;
        total_tat += pcb[i].turnaround_time;
    }

    printf("\nAverage Waiting Time = %.2f\n",
           (float)total_wt / n);
    printf("Average Turnaround Time = %.2f\n",
           (float)total_tat / n);
}

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

    // Setup signal handler untuk timer.
    setup_signal_handler();

    // Aktifkan timer quantum.
    setup_timer();

    // Scheduler belum dijalankan
    while (1) {
        pause();   // Tunggu SIGALRM

        if (quantum_expired) {
            round_robin_scheduler(pcb, MAX_PROCESS);
            display_process_status(pcb, MAX_PROCESS);

            // Cek apakah semua proses FINISHED
            int finished_count = 0;
            for (int i = 0; i < MAX_PROCESS; i++) {
                if (pcb[i].state == FINISHED)
                    finished_count++;
            }

            if (finished_count == MAX_PROCESS) {
                stop_timer();
                print_final_result(pcb, MAX_PROCESS);
                break;
            }
        }
    }

    return 0;
}
