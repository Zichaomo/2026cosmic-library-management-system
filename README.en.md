# 2026 Universe Library Management System

## Project Overview

The 2026 Universe Library Management System is a library management solution developed based on Pure C language, utilizing the Win32 API for the graphical interface. It supports CSV data storage and is suitable for the daily operational management of small to medium-sized libraries.

## Technical Architecture

- **Programming Language**: Pure C, Single Project Structure
- **Graphical Interface**: Win32 API (Supports Mouse Interaction)
- **Data Storage**: CSV Format
- **Development Environment**: Supports Visual Studio Code (C/C++ Configuration)
- **Cross-Platform**: Reserved cross-platform support interfaces

## Main Function Modules

### Core Functions (P0 Level)

1. **Account & Login**
   - User Authentication
   - Permission Management

2. **Graphical Interface Framework**
   - Win32 Window Program
   - Mouse Interaction Implementation

3. **Book Management (Administrator)**
   - Book Information Entry
   - Book Query and Search

4. **Reader Management (Administrator)**
   - Reader Information Management
   - Reader Permission Settings

5. **Borrow/Return/Renewal**
   - Book Borrowing Processing
   - Book Return Management
   - Renewal Functionality

### Extended Functions (P1 Level)

6. **Reservation Function**
   - Book Reservation
   - Reservation Management

7. **Fine Management**
   - Overdue Fines
   - Fine Records

8. **Statistical Reports**
   - Borrowing Statistics
   - Report Generation

## Data Design

The system uses CSV format for data storage, including the following data tables:
- Book Information Table
- Reader Information Table
- Borrowing Record Table
- Reservation Record Table
- Fine Record Table

## Development Environment Configuration

### Visual Studio Code Configuration

The project includes a `.vscode` configuration folder. The following extensions need to be installed:
- C/C++ (Microsoft)
- C/C++ Extension Pack

### Compilation and Execution

```bash
# Compilation Command
gcc -o test.exe test.c -lgdi32 -lcomdlg32

# Or run directly using the provided test.exe
```

## Project Structure

```
├── .vscode/
│   ├── c_cpp_properties.json  # C/C++ Configuration
│   └── tasks.json             # Build Task Configuration
├── docs/
│   └── PRD.md                 # Product Requirements Document
├── test.c                     # Main Program Entry
├── test.exe                   # Executable File
└── README.md                  # Project Description
```

## Milestone Planning

The project is developed in stages step-by-step. The suggested schedule includes:
- Basic Framework Setup
- Core Function Implementation
- Extended Function Development
- Testing and Optimization

## Acceptance Criteria

- All P0 functions operate normally
- P1 functions fully implemented
- System operates stably
- Data storage is correct

## Risks and Assumptions

- Dependent on Windows Platform (Win32 API)
- CSV data storage is suitable for small-scale data
- Further performance testing validation required

## License

This project follows an open-source license agreement.

---

For more technical details, please refer to the complete Product Requirements Document in docs/PRD.md.