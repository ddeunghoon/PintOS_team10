#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* Kwak */
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"

/* Kwak */
struct lock fs_lock;
#define LOCK_FS() lock_acquire(&fs_lock)
#define UNLOCK_FS() lock_release(&fs_lock)
#define FD_LIMIT (1 << 9)


static void syscall_handler (struct intr_frame *);
/* Kwak */
void get_arg_from_stack (void *esp, int *argv, int argc);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* Kwak */
  lock_init(&fs_lock);
}

/* Kwak */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int32_t argv[4];
  if (f == NULL || f->esp == NULL) {
    return;
  }
  uint32_t switch_val = *(uint32_t *)(f->esp);
  switch (switch_val) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      get_arg_from_stack(f->esp,argv,1);
      /* Choi */
      is_valid_address((void*)argv[0]);
      exit((int)argv[0]);
      break;
    case SYS_EXEC:
      get_arg_from_stack(f->esp,argv,1);
      f->eax = exec((int)argv[0]);
      break;
    case SYS_WAIT:
      get_arg_from_stack(f->esp,argv,1);
      f->eax = wait((int)argv[0]);
      break;
    case SYS_CREATE:
      get_arg_from_stack(f->esp,argv,2);      
      is_valid_address((void*)argv[0]);
      f->eax = create((const char*)argv[0],(unsigned)argv[1]);
      break;
    case SYS_REMOVE:
      get_arg_from_stack(f->esp,argv,1);
      is_valid_address((void*)argv[0]);
      f->eax = remove((const char*)argv[0]);
      break;
    case SYS_OPEN:
      get_arg_from_stack(f->esp, argv, 1);
      is_valid_address((void*)argv[0]);
      f->eax = open ((const char*)argv[0]);
      break;
    case SYS_FILESIZE:
      get_arg_from_stack(f->esp,argv,1);
      f->eax=filesize((int)argv[0]);
      break;
    case SYS_READ:
      get_arg_from_stack(f->esp,argv,3);
      is_valid_address((void*)argv[1]);
      f->eax = read((int)argv[0], (void*)argv[1], (unsigned)argv[2]);
      break;
    case SYS_WRITE:
      get_arg_from_stack(f->esp,argv,3);
      f->eax = write((int)argv[0], (void*)argv[1], (unsigned)argv[2]);
      break;
    case SYS_SEEK:
      get_arg_from_stack(f->esp,argv,2);
      seek((int)argv[0], (unsigned)argv[1]);
      break;
    case SYS_TELL:
      get_arg_from_stack(f->esp,argv,1);
      f->eax = tell((int)argv[0]);
      break;
    case SYS_CLOSE:
      get_arg_from_stack(f->esp,argv,1);
      close((int)argv[0]);
      break;
  }
}
void halt (void) {
  shutdown_power_off();
}

void exit (int status) {
  struct thread* cur = thread_current();
  printf("%s: exit(%d)\n", thread_name(), status);
  cur->exit_status = status;
  thread_exit ();
}

pid_t exec (const char *cmd) {
  return process_execute(cmd);
}

int wait (pid_t pid) {
  return process_wait(pid);
}

int read (int n_fd, void* buffer, unsigned size) {
  is_valid_address(buffer);
  struct thread * cur = thread_current();
  int read_len = 0;
  if (n_fd < 0 || n_fd >= FD_LIMIT) return -1;
  
  LOCK_FS();
  
  int i;
  if (n_fd == 0) {
    for (i = 0; i < size; i++) {
      ((char *)buffer)[i] = input_getc();
      read_len++;
      if (((char *)buffer)[i] == '\0') {
        break;
      }
    }
  }
  else {
    struct file *f = cur->fd[n_fd];
    if(f != NULL) read_len = file_read(f,buffer,size);
  }
  UNLOCK_FS();
  return read_len;
}


int write (int n_fd, const void *buf, unsigned size) {
  is_valid_address(buf);
  struct thread * cur = thread_current();
  int len_write = 0;
  if (n_fd < 0 || n_fd >= FD_LIMIT) return -1;

  LOCK_FS();
  if (n_fd == 1) {
    putbuf(buf, size);
    len_write = size;
  }
  else {
    struct file *f = cur->fd[n_fd];
    if(f!=NULL) {
      len_write = file_write(f,buf,size);
    }
  }
  UNLOCK_FS();
  return len_write;
}

bool create(const char *file, unsigned initial_size) {
  if (file == NULL) exit(-1);
  return filesys_create (file,initial_size);
}

bool remove(const char *file) {
  return filesys_remove(file);
}

int open (const char *file) {
  is_valid_address(file);
  struct file * f = NULL;
  struct thread * cur = thread_current();
  if(file == NULL) return -1;
  LOCK_FS();
  f = filesys_open(file);
  if(f == NULL) {
    UNLOCK_FS();
    return -1;
  } else {
    int i;
    for (i = 3; i < 128; i++) {
        if (cur->fd[i] != NULL) continue;
        if (strcmp(thread_current()->name, file) == 0) {
            file_deny_write(f);
        }
        cur->fd[i] = f;
        UNLOCK_FS();
        return i;
    }   
  }
  file_close(f);
  UNLOCK_FS();
  return -1;
}

int filesize(int n_fd) {
  struct thread * cur = thread_current();
  struct file * f = cur->fd[n_fd];
  if(f == NULL) return -1;
  return file_length(f);
}
void seek (int n_fd, unsigned posit) {
  if (thread_current()->fd[n_fd] == NULL) exit(-1);
  file_seek(thread_current()->fd[n_fd], posit);
}

unsigned tell (int n_fd) {
  if (thread_current()->fd[n_fd] == NULL) exit(-1);
  return file_tell(thread_current()->fd[n_fd]);
}

void close (int n_fd) {
  struct file* fp;
  struct thread * cur = thread_current();
  if (n_fd < 0 || n_fd >= FD_LIMIT) return -1;
  if (cur->fd[n_fd]==NULL) exit(-1);
  fp = cur->fd[n_fd];
  cur->fd[n_fd]=NULL;
  return file_close(fp);
}

void is_valid_address(void *addr){
  if(!is_user_vaddr(addr)) exit(-1);
}

#define READ_ARG(ESP, ARGV, INDEX) \
    do { \
        is_valid_address((ESP)+4 * (INDEX)); \
        (ARGV)[INDEX] = *((int*)((ESP)+4 * (INDEX)+4)); \
    } while (0)

void get_arg_from_stack(void *esp, int *argv, int argc) {
    int i;
    for (i=0; i <argc; i++){
        READ_ARG(esp, argv, i);
    }
}