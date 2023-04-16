#ifndef __UTIL_H__
#define __UTIL_H__

#define and &&
#define or ||

#define ksh_write_fd 1

enum KBbutton{UP=74751, DOWN, RIGHT, LEFT, BACKSPACE, HM, END, DEL, ORDINARY, UNKOWN};

extern void init_tty_mode();
extern void reset_tty_mode();
extern int fork1();
extern void kexit(int status, int ifsay, char *msg);
extern void panic(char *s);
extern int FAKEKBDDVR_getbutton(int fd, char *keytype);
extern void fprintf(int fd, const char *fmt, ...);
extern void warn(int fd, const char *fmt, ...);

#endif