#ifndef COSMIC_MIGRATION_LIBRARY_H
#define COSMIC_MIGRATION_LIBRARY_H
#include "migration/auth.h"
enum { CL_LIB_OK,CL_LIB_PENDING,CL_LIB_STORAGE,CL_LIB_UNCERTAIN,CL_LIB_DENIED,
       CL_LIB_NOT_FOUND,CL_LIB_BLOCKED,CL_LIB_NO_COPY,CL_LIB_LIMIT,
       CL_LIB_DUPLICATE,CL_LIB_CLOSED,CL_LIB_OVERDUE,CL_LIB_RENEW_LIMIT,
       CL_LIB_QUEUE,CL_LIB_AVAILABLE,CL_LIB_INVALID,CL_LIB_CONFLICT };
typedef struct {
    int id;
    const char *isbn,*title,*author;
    const char *original_isbn,*original_title,*original_author;
} ClBookEdit;
int cl_library_edit_book(ClStore *s,const ClSession *actor,const ClBookEdit *edit,ClDate today);
const char *cl_library_message(int result);
int cl_library_refresh(ClStore *s,ClDate today);
int cl_library_borrow(ClStore *s,const ClSession *actor,int reader,int book,ClDate today,int *loan);
int cl_library_return(ClStore *s,const ClSession *actor,int loan,ClDate today,int *fine);
int cl_library_renew(ClStore *s,const ClSession *actor,int loan,ClDate today);
int cl_library_reserve(ClStore *s,const ClSession *actor,int reader,int book,ClDate today);
int cl_library_cancel(ClStore *s,const ClSession *actor,int reservation,ClDate today);
int cl_library_pay(ClStore *s,const ClSession *actor,int fine,ClDate today);
int cl_library_add_book(ClStore *s,const ClSession *actor,const char *isbn,const char *title,
                        const char *author,int copies,ClDate today);
int cl_library_book_action(ClStore *s,const ClSession *actor,int book,int add_copy,ClDate today);
int cl_library_reader_action(ClStore *s,const ClSession *actor,int reader,ClDate today);
int cl_library_seed(ClStore *s,const ClSession *actor,ClDate today);
int cl_library_find(const ClStore *s,int kind,int id);
int cl_library_number(const ClRow *row,int field);
int cl_library_overdue(const ClRow *loan,ClDate today);
#endif
