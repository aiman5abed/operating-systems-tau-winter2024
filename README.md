# operating-systems-tau-winter2024
This repository contains my completed programming assignments for the **Operating Systems** course (0512.4402) at **Tel Aviv University**.

The projects demonstrate practical knowledge in:
- Process and thread management
- Synchronization primitives
- File and I/O systems
- TCP/IP socket programming
- Low-level C programming on Linux

---

## Course Information
 
- **Institution**: Tel Aviv University  
- **Language**: C under Linux  
- **Topics**: Processes, Memory, Scheduling, I/O, Filesystems, Network communication

> See [`syllabus.pdf`](./syllabus.pdf) for full course outline.

---

## Projects Overview

### [HW1: Linux Shell](./hw1_shell/)
**Topic**: Process control, job management  
**Summary**: Developed a custom Linux shell supporting background tasks, job tracking, and internal commands (`cd`, `exit`, `jobs`) using `fork`, `exec`, and `waitpid`.

### [HW2: Dispatcher/Worker Thread Model](./hw2_dispatcher/)
**Topic**: Multithreading, synchronization  
**Summary**: Built a dispatcher that spawns worker threads to process file-based tasks concurrently. Includes logging, command parsing, and turnaround-time metrics.

### [HW3: Multi-Client TCP Chat Server](./hw3_chat_server/)
**Topic**: Socket programming, concurrency  
**Summary**: Designed a TCP-based client-server application. Multiple clients can broadcast or send private messages. Implemented asynchronous message handling and graceful disconnects.

---

## Key Technical Highlights

- **C Programming**
  - System calls (`fork`, `execvp`, `select`, `pthread`, `open`, `read`, `write`)
- **Thread Synchronization**
  - Mutexes, condition variables, and producer-consumer logic
- **Network Communication**
  - Socket APIs: `bind`, `listen`, `accept`, `connect`, `send`, `recv`
- **Linux Systems**
  - File descriptors, process states, background jobs, and more

---

## How to Build and Run

### Build all
```bash
make all
Run HW1 Shell
cd hw1_shell
make
./hw1shell
Run HW2 Dispatcher
cd hw2_dispatcher
make
./hw2 test.txt 4 10 1
Run HW3 Chat Server/Client
cd hw3_chat_server
make
./hw3server 12345
./hw3client 127.0.0.1 12345 Aiman
