#ifndef COSMIC_NEXT_APP_H
#define COSMIC_NEXT_APP_H
#include "ui_next/terminal.h"
#include "migration/library.h"
typedef enum { N_WELCOME,N_LOGIN,N_REGISTER,N_ADMIN,N_READER,N_CATALOG,N_MY_LOANS,
    N_MY_RESERVATIONS,N_MY_FINES,N_BOOKS,N_NEW_BOOK,N_READERS,N_LOANS,N_BORROW,
    N_RESERVATIONS,N_FINES,N_STATS,N_PAGE_COUNT } NextPage;
enum { CMD_NONE,CMD_BORROW,CMD_RETURN,CMD_RENEW,CMD_RESERVE,CMD_CANCEL,CMD_PAY,CMD_COPY,CMD_SUSPEND,CMD_FREEZE };
enum { LOANS_ALL,LOANS_ACTIVE,LOANS_RETURNED,LOANS_OVERDUE,LOANS_SOON,LOANS_ATTENTION,LOANS_FILTER_COUNT };
typedef struct {
    ClStore store;ClSession session;ClDate today;
    NextPage page;Edit field[5],search;
    int running,focus,selected,matches[CL_TABLE_LIMIT],match_count,kind;
    int searching,help,detail,dirty,discard,form_reserve;
    int command,command_id,command_reader;
    int loan_filter,loan_sort,sort_work[CL_TABLE_LIMIT];
    int edit_book_id,borrow_book_id,borrow_reader_id;
    char edit_original[3][512];
    int picker,picker_submit,picker_count,picker_selected,picker_ids[CL_TABLE_LIMIT];
    char message[512];
} NextApp;
int cl_app_init(NextApp *app,const char *directory,ClDate today);
void cl_app_dispose(NextApp *app);
void cl_app_enter(NextApp *app,NextPage page);
void cl_app_event(NextApp *app,Event event,ClDate today);
void cl_app_rebuild(NextApp *app);
void cl_app_scene(const NextApp *app);
int cl_app_selected_id(const NextApp *app);
const char *cl_app_page_title(NextPage page);
int cl_app_loan_days(const ClRow *loan,ClDate today,int *days);
int cl_app_filter_loan(const NextApp *app,const ClRow *loan);
void cl_app_sort_loans(NextApp *app);
void cl_app_due_counts(const NextApp *app,int *soon,int *overdue);
const char *cl_app_filter_name(int filter);
const char *cl_app_sort_name(int sort);
#endif
