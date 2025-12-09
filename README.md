# Windows Task Manager

A Windows Task Manager–style desktop app for process management, performance monitoring, and service control.

## Features
- View, sort, and filter running processes (PID, CPU, RAM, GPU, Disk I/O)
- Terminate processes
- Inspect process details: modules, threads, open handles, command line etc.
- Real-time performance monitoring (CPU, memory, disk, network, GPU)
- GPU metrics via ADL (AMD) and NVML (NVIDIA)
- Windows Services: list, start/stop, view description
- Performance charts 
- Monitoring single CPU core usage

## Tech Stack
- Language/GUI: C++, Qt
- OS APIs: Windows API (process, threads, services, performance counters)
- GPU Telemetry: ADL (AMD), NVML (NVIDIA)

## Architecture
- Core: wrappers over WinAPI for processes, adapters, services, definition of performance monitoring interfaces
- Monitoring: implementation of performance monitoring interfaces and process and service control interfaces using Windows API and ADL/NVML
- Data update: single thread which calls monitors interfaces and collects data for UI update 
- UI: Qt widgets for tables, details panes, QtCharts for charts. UI depends on core interfaces and data structs, not from Windows API implementation, so the app is easy to extension
