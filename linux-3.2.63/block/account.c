#include <linux/account.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/string.h>

static struct timer_list lru_timer;
struct list_head vm_task_list;
spinlock_t vm_task_lock;
static struct kobject *vm_list_kobj;
/*struct kobj_attribute vm_task_attr={
    .attr={"vm_task_list",0660},
    .show=NULL,
    .store=&traced_task_add,
};*/
//struct timeval last_time_val;
static unsigned int math_pow(unsigned int a, unsigned int n);
void start_lru_timer(void)
{
    INIT_LIST_HEAD(&vm_task_list);
    init_timer(&lru_timer);
    lru_timer.function = lru_timer_fn;
    add_timer(&lru_timer);
    mod_timer(&lru_timer,jiffies+HZ*10);
    return;
}
//EXPORT_SYMBOL(start_lru_timer);

void lru_timer_fn(void)
{
    //gettimeofday(&last_time_val, NULL);
    struct task_account *tsk_acct;
    struct task_struct *task;
    int i;
    i=0;
    if(spin_trylock(&vm_task_lock))
    {
        printk(KERN_INFO"vm_task_lock locked\n");
        list_for_each_entry(tsk_acct,&vm_task_list,list){
            if(!tsk_acct->task){
                printk(KERN_INFO"task %d %s is deleted!!\n",tsk_acct->pid,tsk_acct->name);
                __list_del_entry(&tsk_acct->list);
                kfree(tsk_acct);
                continue;
            }
            else if(strcmp(tsk_acct->task->comm,tsk_acct->name)!=0||tsk_acct->pid!=tsk_acct->task->pid){
                tsk_acct->task->traced=0;
                tsk_acct->task->acct=NULL;
                printk(KERN_INFO"task %d %s is deleted!! total_pages_in_lru=%d\n",tsk_acct->pid,tsk_acct->name,tsk_acct->total_pages_in_lru);
                __list_del_entry(&tsk_acct->list);
                kfree(tsk_acct);
                continue;
            }
            printk(KERN_INFO"task %d traced,name=%s\n",tsk_acct->task->pid,tsk_acct->task->comm);
            if(spin_trylock(&tsk_acct->lock)){
                do_gettimeofday(&tsk_acct->last_time_val);
                task=tsk_acct->task;
                printk(KERN_INFO"%d %s: total_pages_in_lru=%d,total_pages=%d\n",task->pid,task->comm,tsk_acct->total_pages_in_lru,tsk_acct->total_pages);
                tsk_acct->total_pages=0;
                spin_unlock(&tsk_acct->lock);
            }
            i++;
        }
        spin_unlock(&vm_task_lock);
    }
    //printk(KERN_INFO"next timer of lru i=%d\n",i);
    mod_timer(&lru_timer,jiffies+HZ*10);
    
}
//EXPORT_SYMBOL(lru_timer_fn);

unsigned long timeval_minus(struct timeval a1, struct timeval a2)
{
    unsigned long result;
    result=timeval_to_jiffies(&a1)-timeval_to_jiffies(&a2);
    return result;
}
//EXPORT_SYMBOL(timeval_minus);

void free_pages_account(struct page *page, unsigned int order)
{
    int i;
    unsigned int times = math_pow(2,order);
    for(i=0;i<times;i++){
        if (page->pg_acct && page->pg_acct->task->traced && page->pg_acct->task->acct){
            spin_lock(&page->pg_acct->task->acct->lock);
            page->pg_acct->task->acct->total_pages_in_lru--;
            spin_unlock(&page->pg_acct->task->acct->lock);
            kfree(page->pg_acct);    
        }
    page++;
    }
}
//EXPORT_SYMBOL(free_pages_account);

void free_page_account(struct page *page)
{
    if (page->pg_acct && page->pg_acct->task->traced && page->pg_acct->task->acct){
        spin_lock(&page->pg_acct->task->acct->lock);
        if(page->pg_acct->page_in_lru==1)
            page->pg_acct->task->acct->total_pages_in_lru--;
        else
            printk(KERN_INFO"free a page of traced fn not in lru\n");
        spin_unlock(&page->pg_acct->task->acct->lock);
        kfree(page->pg_acct);
        page->pg_acct=NULL;
    }
 
}

static unsigned int math_pow(unsigned int a, unsigned int n)
{
    int i;
    int result=1;
    if(n==0)
        return result;
    else{
        for(i=0;i<n;i++)
            result*=a;
    }
    return result;
}

static ssize_t traced_task_add(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    char *end;
    struct task_struct *new_pid_task;
    unsigned long new_pid_num = simple_strtoul(buf, &end, 10);
    printk(KERN_INFO"new_pid_num=%d\n",new_pid_num);
    new_pid_task = find_task_by_vpid((int)new_pid_num);
    printk(KERN_INFO"new_pid_task pid=%d,tgid=%d,name=%s\n",new_pid_task->pid,new_pid_task->tgid,new_pid_task->comm);
    if (!new_pid_task->acct){
        new_pid_task->acct = kmalloc(sizeof(struct task_account),GFP_ATOMIC);
        spin_lock_init(&new_pid_task->acct->lock);
        spin_lock(&new_pid_task->acct->lock);
        new_pid_task->acct->total_pages_in_lru=0;
        new_pid_task->acct->total_pages=0;
        new_pid_task->acct->task=new_pid_task;
        strcpy(new_pid_task->acct->name,new_pid_task->comm);
        new_pid_task->acct->pid=new_pid_task->pid;
        printk(KERN_INFO"new_pid_task pid=%d,tgid=%d\n",new_pid_task->pid,new_pid_task->tgid);

    }
    else{
        printk(KERN_INFO"new_pid_task already has acct,acct->task=%d\n",new_pid_task->acct->task->pid);
        spin_lock(&new_pid_task->acct->lock);
        new_pid_task->acct->total_pages=0;
        
    }
    spin_unlock(&new_pid_task->acct->lock);
    spin_lock(&vm_task_lock);
    list_add(&new_pid_task->acct->list,&vm_task_list);
    spin_unlock(&vm_task_lock);
    new_pid_task->traced=1;
    return count;
}

struct kobj_attribute vm_task_attr={
    .attr={"vm_task_list",0660},
    .show=NULL,
    .store=&traced_task_add,
};

static struct attribute *vm_task_attr_list[]={
    &vm_task_attr.attr,
    NULL,
};

static struct attribute_group vm_task_attr_list_group={
    .attrs = vm_task_attr_list,
};

void init_task_tracing_obj(void)
{
    spin_lock_init(&vm_task_lock);
    vm_list_kobj = kobject_create_and_add("vm_list",NULL);
    if(vm_list_kobj)
        sysfs_create_group(vm_list_kobj,&vm_task_attr_list_group);
}
