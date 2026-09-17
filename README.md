# Real-Time Greenhouse Control System

Academic project developed for the **Real-Time Systems** course in the Computer Engineering program at the **Federal University of Santa Maria (UFSM)**.

The project implements a client-server greenhouse monitoring and control system. The server is written in **C** and uses POSIX concurrency primitives to coordinate periodic tasks and shared state. A **Python** client provides a user-facing interface and communicates with the server over TCP sockets.

## Highlights

-- Multi-threaded server in C using **POSIX Threads**
-- Synchronization with **mutexes**, **condition variables**, and atomic state
-- Periodic task execution
-- TCP socket communication
-- Python client interface
-- Separation between monitoring, control, communication, and user interaction

## Technologies

`C` · `Python` · `POSIX Threads` · `TCP/IP` · `Sockets` · `Concurrency` · `Synchronization` · `Make`

## Repository structure

```text
.
├── server/
│   ├── servidor.c
│   └── Makefile
├── client/
│   └── cliente.py
└── README.md
```

## Academic context

This repository contains the files from my final academic work in Real-Time Systems. It is published as a portfolio artifact to demonstrate the architecture and concepts used in the project.

## Author

**Angelo Luigi Bocchi Lovatto**  
Computer Engineering — UFSM
