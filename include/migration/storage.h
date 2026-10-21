#ifndef COSMIC_MIGRATION_STORAGE_H
#define COSMIC_MIGRATION_STORAGE_H
#include "utils/foundation.h"

enum { CL_READERS,CL_BOOKS,CL_LOANS,CL_FINES,CL_RESERVATIONS,CL_TABLES,
       CL_CSV_COLUMNS=9,CL_CSV_BYTES=512,CL_TABLE_LIMIT=10000,CL_PATH_BYTES=512 };
typedef struct { char field[CL_CSV_COLUMNS][CL_CSV_BYTES];int count; } ClRow;
typedef struct { ClRow *rows;int count,capacity; } ClTable;
typedef struct { FILE *file;int pending; } ClCsvReader;
extern const char *const cl_csv_names[CL_TABLES];
extern const char *const cl_csv_headers[CL_TABLES];
extern const int cl_csv_counts[CL_TABLES];

/* read: 1=record, 0=clean EOF, -1=malformed/I/O error. Out is atomic.
   Call begin at the start of a seekable file; it skips an optional BOM.
   UTF-8 bytes are retained, including CR/LF inside quoted fields. */
int cl_csv_begin(ClCsvReader *reader,FILE *file);
int cl_csv_read(ClCsvReader *reader,ClRow *out);
int cl_csv_write(FILE *file,const ClRow *row);
int cl_table_append(ClTable *table,const ClRow *row);
void cl_table_free(ClTable *table);
int cl_table_load(ClTable *table,const char *path,int kind);
int cl_table_write(const ClTable *table,const char *path,int kind);
int cl_store_validate(const ClTable tables[CL_TABLES]);

typedef struct {
    ClTable table[CL_TABLES];
    char directory[CL_PATH_BYTES];
    int generation,active_slot,blocked,pending_export,recovered;
} ClStore;
enum { CL_STORE_UNCERTAIN=-1,CL_STORE_FAILED=0,CL_STORE_OK=1,
       CL_STORE_PENDING_EXPORT=2 };
/* Single-process only. Directory and all five CSV files + state.0/state.1
   must be explicitly prepared externally. ASCII paths; file contents UTF-8.
   After migration, snapshots are authoritative; do not edit/run legacy writers
   on these files. open repairs all CSV exports before permitting operations.
   Initialize output with {0}; dispose before reopening the same object. */
int cl_store_open(ClStore *store,const char *directory);
void cl_store_dispose(ClStore *store);
/* Call with edited tables. FAILED means no commit; caller undoes memory edit.
   UNCERTAIN blocks use: dispose/reopen before deciding whether to retry.
   PENDING_EXPORT is committed successfully; do not repeat the operation. */
int cl_store_commit(ClStore *store);
#ifdef CL_MIGRATION_TEST
/* Crash boundaries, compiled out of the normal implementation. */
void cl_store_test_stop(int stage);
#endif
#endif
