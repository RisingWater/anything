# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Anything is a fast file search tool for Linux desktop (inspired by Windows Everything). It has a Qt5 desktop client, a C++ backend service that indexes files into SQLite, and an audisp plugin that hooks into Linux auditd for real-time filesystem change notifications.

## Build

```bash
cd docker && docker build -t anything-build .
docker run -it --rm -v $HOME:/workdir anything-build /bin/bash
mkdir build && cd build
cmake .. -G Ninja
ninja package
```

The `.deb` package lands in `build/`. Build requires g++-8, CMake 3.8+, Qt5, libsqlite3, libaudit, ninja.

## Architecture

```
audisp-anything-plugin          Qt5 client (anything)
(stdin from auditd)             (QMainWindow + tray icon)
       |                              |
       | POST /api/audit/events       | REST calls to localhost:5071
       v                              v
       +--------- Crow HTTP server (anything_file_service) ---------+
                 port 5071, multithreaded
                 |
                 +-- WebService  (route handler, JSON I/O)
                 +-- FileScannerManager (singleton, manages per-directory scanners)
                 +-- FileScanner        (recursive dir walk → FileDB)
                 +-- FileWatcher        (Linux audit rules for inotify-like change detection)
                 +-- FileDB             (SQLite: file_info table, search tasks)
                 +-- ScanObject         (SQLite: scan_obj table, which dirs to scan)
                 +-- DBManager          (SQLite connection pool, one DB per UID)
```

**Key data flow**: Client POSTs `/api/filedb/{uid}/task/{search_text}` → server creates a `SearchTask` that returns results in batches → client polls `GET /api/filedb/{uid}/task/{task_id}` with progress until `COMPLETED`.

**Database layout**: SQLite files live at `/opt/apps/com.anything/files/db/<uid>/file_db.db`. Two logical table groups: `file_info` (indexed file metadata) and `scan_obj` (which directories to scan, per user). Each UID gets its own database file.

**Server entry** (`server/main.cpp`): Instantiates `crow::SimpleApp`, wires `WebService` routes, calls `FileScannerManager::getInstance().initializeAllScanners()`, listens on port 5071.

**Client entry** (`client/main.cpp`): `SingleInstanceApp` ensures only one process, then `FileSearchApp` (main window) with system tray, search debounce timer, batch polling of search results.

**audisp plugin** (`server/audisp-anything-plugin/Audisp.cpp`): Reads auditd pipe from stdin, parses `type=PATH` lines for CREATE/DELETE events, POSTs JSON to `/api/audit/events`.

## Key dependencies (vendored)

- `3rdparty/crow/include/crow.h` — Crow HTTP server (header-only)
- `3rdparty/asio/include/` — Asio (standalone, header-only)
