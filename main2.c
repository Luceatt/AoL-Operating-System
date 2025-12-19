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
int running_time = TIME_QUANTUM; // berapa lama process berjalan

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

    // ---------------------- tambah variable----------------------------
    int in_queue; // Penanda apakah program sudah di queue atau belum
    // ------------------------ selesai --------------------------------
} PCB;

//---------------------------- tambah fungsi disini ---------------------------

// Membuat simple queue dan fungsi fungsinya
typedef struct {
    int data[MAX_PROCESS];
    int front;
    int rear;
    int count;
} ReadyQueue;

// Fungsi inisialisasi queue
void init_queue(ReadyQueue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

// Funtuk cek apakah queue kosong atau tidak
int is_empty(ReadyQueue *q) {
    return q->count == 0;
}

// Fungsi untuk cek apakah queue penuh atau tidak
int is_full(ReadyQueue *q) {
    return q->count == MAX_PROCESS;
}

// Fungsi untuk memasukan index kedalam queue
void enqueue(ReadyQueue *q, int idx) {
    if (is_full(q)) return;
    q->rear = (q->rear + 1) % MAX_PROCESS;
    q->data[q->rear] = idx;
    q->count++;
}

// Fungsi untuk mengeluarkan index dari queue
int dequeue(ReadyQueue *q) {
    if (is_empty(q)) return -1;
    int idx = q->data[q->front];
    q->front = (q->front + 1) % MAX_PROCESS;
    q->count--;
    return idx;
}

// inisialisasi variable queuenya
ReadyQueue ready_queue;

//------------------------------------------ Selesai penambahan ---------------------------------


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
        raise(SIGSTOP);                  // Child process distop dulu supaya tidak langsung jalan, harus tunggu scheduler.

        child_process_work(process_id);  // Simulasi kerja process pakai function child_process_work().
        exit(0);                         // Child process diterminate saat child_process_work() selesai.
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

    pcb.in_queue = 0; // set process tidak di queue
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
    timer.it_value.tv_sec = running_time;
    timer.it_value.tv_usec = 0;

    // Interval SIGALRM berikutnya
    timer.it_interval.tv_sec = running_time;
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

// ROUND ROBIN Scheduler Function:
// Untuk mengecek apakah ada process yang baru datang, lalu memasukannya di queue.
void check_new_arrivals(PCB pcb[], int n) {
    for (int i = 0; i < n; i++) {   // Loop sampai max process.
        // Kondisi: cek process yang sedang nunggu (arrival timenya <= current time) dan berada di dalam READY state,
        // tapi belum pernah masuk ke ready queue.
        if (pcb[i].arrival_time <= current_time && pcb[i].state == READY && pcb[i].in_queue == 0) {
            enqueue(&ready_queue, i);   // Masukin process tersebut ke queue.
            pcb[i].in_queue = 1;        // Nyalakan flagnya, artinya process sudah masuk ke queue.
        }
    }
}

// Function untuk hitung waiting time tiap process.
void update_waiting_time(PCB pcb[], int n) {
    for (int i = 0; i < n; i++) {   // Loop sampai max process.
        // Kondisi: cek process yang sedang nunggu (arrival timenya <= current time) dan berada di dalam READY state.
        // Bukan untuk process yang sedang RUNNING (current process) atau process yang sudah FINISHED.
        if (pcb[i].state == READY && pcb[i].arrival_time <= current_time && i != current_process) {
            pcb[i].waiting_time += running_time;    // Tambahkan waiting time process tersebut dengan running time sekarang.
        }
    }
}

// Function untuk menandai bahwa process sudah selesai dikerjakan.
void finish_process(PCB pcb[], int index) {
    pcb[index].state = FINISHED;            // Set statenya ke FINISHED.
    pcb[index].finish_time = current_time;  // Set finish timenya ke current time.

    // Set TAT dengan rumus: TAT = Completion Time - Arrival Time.
    pcb[index].turnaround_time = pcb[index].finish_time - pcb[index].arrival_time;
    // Set waiting time dengan rumus: WT = Turnaround Time - Burst Time.
    pcb[index].waiting_time = pcb[index].turnaround_time - pcb[index].burst_time;
}

// Function untuk jalanin Round Robin Scheduler, akan dipanggil jika time quantumnya habis.
void round_robin_scheduler(PCB pcb[], int n) {
    check_new_arrivals(pcb, n); // Cek apakah ada process baru datang?
    
    // Kondisi 1: cek apakah ada proses yang sedang RUNNING. Jika iya:
    if (current_process != -1) {
        kill(pcb[current_process].pid, SIGSTOP);                // Stop sementara process yang sedang RUNNING.
        pcb[current_process].remaining_time -= running_time;    // Kurangin sisa execution time proses tersebut dengan running time.

        update_waiting_time(pcb, n);    // Update waiting time proses lain.
        current_time += running_time;   // Update waktu system sekarang.
        
        // Jika execution time dari processnya habis, maka:
        if (pcb[current_process].remaining_time <= 0) {
            finish_process(pcb, current_process);   // Tandain bahwa process tersebut sudah selesai dieksekusi.
        } else {
            // Jika belum selesai, maka kembalikan process itu dari state RUNNING ke state READY.
            pcb[current_process].state = READY;
            check_new_arrivals(pcb, n);             // Cek lagi apakah ada process baru yang datang di waktu sekarang?
            enqueue(&ready_queue,current_process);  // Jika masih ada sisa remaining time, maka masukkan kembali ke ready queue.
            pcb[current_process].in_queue = 1;      // Tandai kalau process sudah berada di ready queue.
        }
    }

    // Kondisi 2: jika queue empty, maka tidak ada process yang bisa dijalankan.
    if (is_empty(&ready_queue) ){
        current_process = -1;   // Set CPU jadi idle.
        return;
    }

    // Ambil process selanjutnya dari ready queue.
    current_process = dequeue(&ready_queue);

    // Tentuin lama process yang akan dijalankan.
    // Jika remaining timenya < time quantum, maka running timenya akan diganti sesuai remaining time.
    running_time = (pcb[current_process].remaining_time < TIME_QUANTUM) ? pcb[current_process].remaining_time : TIME_QUANTUM;

    pcb[current_process].state = RUNNING;       // Ubah state process dari READY jadi RUNNING.
    kill(pcb[current_process].pid, SIGCONT);    // Lanjut eksekusi process.

    quantum_expired = 0;    // Reset flag time quantum jadi 0.
}

// Function untuk display status saat ini dari suatu process.
void display_process_status(PCB pcb[], int n) {
    printf("\n[TIME %d]\n", current_time);
    for (int i = 0; i < n; i++) {
        printf("P%d | State: %d | Remaining: %d | Waiting: %d\n",
               pcb[i].id, pcb[i].state, pcb[i].remaining_time, pcb[i].waiting_time);
    }
}

// Function untuk display info dan hasil akhir tiap process.
void print_final_result(PCB pcb[], int n) {
    // Initialize variable untuk simpan total waiting time dan total turnaround time.
    int total_wt = 0;
    int total_tat = 0;

    printf("\nFinal Result:\n");
    printf("PID | AT | BT | WT | TAT\n");

    // Display arrival time, burst time, waiting time, dan turnaround time tiap process.
    for (int i = 0; i < n; i++) {
        printf("P%d  | %d  | %d  | %d  | %d\n",
                pcb[i].id, pcb[i].arrival_time, pcb[i].burst_time, pcb[i].waiting_time, pcb[i].turnaround_time);

        total_wt += pcb[i].waiting_time;
        total_tat += pcb[i].turnaround_time;
    }

    // Hitung dan display average waiting time dan average turnaround time.
    printf("\nAverage Waiting Time = %.2f\n",
           (float)total_wt / n);
    printf("Average Turnaround Time = %.2f\n",
           (float)total_tat / n);
}

// Main program.
int main() {
    printf("Round Robin Scheduler Simulation\n");
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

    // Buat queuenya
    init_queue(&ready_queue);

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

    setup_signal_handler(); // Setup signal handler untuk timer.
    setup_timer();          // Aktifkan timer quantum.

    // Loop untuk scheduler, jalan sampai semua process selesai.
    while (1) {
        pause();   // Tunggu SIGALRM (timer interrupt).

        // Jika time quantum habis:
        if (quantum_expired) {
            round_robin_scheduler(pcb, MAX_PROCESS);    // Jalanin Round Robin Scheduler untuk pilih next process.
            display_process_status(pcb, MAX_PROCESS);   // Display status seluruh process sekarang.

            // Hitung jumlah proses yang dalam state FINISHED.
            int finished_count = 0;
            for (int i = 0; i < MAX_PROCESS; i++) {
                if (pcb[i].state == FINISHED)
                    finished_count++;
            }

            // Jika semua sudah FINISHED, maka stop timernya dan tampilin hasil akhir dari Round Robin Scheduler Simulation ini.
            if (finished_count == MAX_PROCESS) {
                stop_timer();
                print_final_result(pcb, MAX_PROCESS);
                break;  // Keluar dari loop scheduler.
            }
        }
    }

    return 0;   // Keluar dari program.
}
