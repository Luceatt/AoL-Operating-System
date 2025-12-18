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

#define Max_Process 5 // deinisikan maksimal prosesnya
#define Time_Quantum 2 // definisikan time quantumnya sebanyak 2 detik

PCB pcb[Max_Process];
int current_time = 0;
int finished_count = 0;

// ------------------ CHILD PROCESS ----------------------
void child_process(){
	while(true){
		pause(); // DI pause karena menunggu sigcont atau sigterm	
	}
}

// ------------- Main ------------------
int main(){
	// Inisialisasi sesuai dengan contoh soal
	int arrival[Max_Process] = {0,1,2,3,4};
	int burst[Max_Process] = {5,3,1,2,3};
	
	for (int i = 0; i < Max_Process;i++){
		//inisiate pcb sesuai dengan data
		pcb[i].id = i + 1;
		pcb[i].state = READY;
		pcb[i].arrival_time = arrival[i];
		pcb[i].burst_time = burst[i];
		pcb[i].remaining_time = burst[i];
		
		// memanggil program
		pid_t pid = fork();
		if (pid == 0){
			child_process();
			exit(0);
		} else{
			pcb[i].pid = pid;
			kill(pid,SIGSTOP); // memastikan bahwa child processnya tidak jalan dulu
		}
	}
	
	printf("\n====== ROUND ROBIN SCHEDULING ===========\n");
	printf("Time Quantum = %d detik\n\n",Time_Quantum);
	
	// --------- LOOP SAMPAI SEMUA PROCESS SELESAI -------------
	
	while (finished_count < Max_Process){
		bool executed = flase; // menandakan belum ada eksekusi
		
		for (int i = 0; i < Max_Process;i++){
			if (pcb[i].arrival_time <= current_time && pcb[i].state == READY && pcb[i].remaining_time > 0){
				pcb[i].state = RUNNING; // Jika semua syarat terpenuhi maka process akan dijalankan
				
				// Menentukan seberapa banyak execution timenya, jika remaining timenya lebih kecil dari time quantum
				// maka process akan berjalan selama remaining time
				int exec_time = (pcb[i].remaining_time < Time_Quantum) ? pcb[i].remaining_time : Time_Quantum;
				
				printf("[TIME %d] P%d RUNNING (%d detik)\n", current_time,pcb[i].id,exec_time);
				
				kill(pcb[i].pid,SIGCONT); // Jalankan menggunakan SIGCONT
				sleep(exec_time); // Set timer selama exec_time
				kill(pcb[i].pid,SIGSTOP); // Setelah timer selesai process di stop menggunakan SIGSTOP
				
				pcb[i].remaining_time -= exec_time; // Update remaining time process
				current_time += exec_time; // Update current time
				
				// Jika process sudah selesai maka:
				if(pcb[i].remaining_time == 0){
					pcb[i].state = FINISHED; // Ubah status jadi FINISHED
					pcb[i].finish_time = current_time; // laporkan kapan finish timenya
					kill(pcb[i].pid,SIGTERM); // terminate child programnya
					finished_count++; // tambahkan berapa banyak program yang sudah selesai
					pcb[i].turnaround_time = pcb[i].finish_time - pcb[i].arrival_time; // Hitung TAT nya
					pcb[i].waiting_time = pcb[i].turnaround_time - pcb[i].burst_time; // Hitung wait timenya
					
					
					printf("[TIME %d] P%d FINISHED\n", current_time,pcb[i].id);
				} 
				// jika process belum selesai
				else{
					pcb[i].state = READY; // status process kembali ke ready
				}
				executed = true; // menandakan terjadinya eksekusi
			}
		}
		// jika tidak ada program yang ready ataupun arrive, waktu maju satu;
		if (!executed){
			current_time++;
		}
	}
	
	// berishkan child process jika semua sudah selesai
	for (int i = 0; i < Max_Process;i++){
		wait(NULL);
	}
	
	printf("\n ======= SIMULASI SELESAI =======\n")
	return 0;
}



