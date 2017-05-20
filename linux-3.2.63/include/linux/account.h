#ifndef __ACCOUNT_
#define __ACCOUNT_
#include <linux/types.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#define TASK_COMM_LEN 16

struct task_account {
  //  int traced;
    struct task_struct *task;
    int total_pages;
    int last_total_pages;
    int total_pages_in_lru;
    struct timeval last_time_val;
    struct list_head list; 
    spinlock_t lock;
    char name[TASK_COMM_LEN];
    int pid;
};

struct page_account {
    
    //int used;
    struct task_struct* task;
    struct timeval used_tv;
    int page_in_lru;

};
void start_lru_timer(void);
void lru_timer_fn(void);
unsigned long timeval_minus(struct timeval a1, struct timeval a2);
void free_pages_account(struct page *page, unsigned int order);
void init_task_tracing_obj(void);
void free_page_account(struct page *page);


#endif
