#include <myheaders.h>
#include <util.h>

static struct termios origin_attr;
void init_tty_mode()
{
	struct termios attr;
	static int i=0;
	tcgetattr(0, &attr);
	if(!i) origin_attr=attr, i=1;
	attr.c_lflag &= ~(ICANON | ECHO);
	attr.c_cc[VTIME] = 0;
	attr.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &attr);
}
void reset_tty_mode()
{
	tcsetattr(0, TCSANOW, &origin_attr);
}

int fork1()
{
	int pid;
	pid = fork();
	if(pid == -1) panic("fork error.");
	return pid;
}
void kexit(int status, int ifsay, char *msg)
{
	reset_tty_mode();
	if(ifsay)
		fprintf(2, "\n\e[1m\e[33m%s %d.\e[0m\e[0m\n\n", msg, status);
	_exit(status);
}
void panic(char *s)
{
	fprintf(2, "\n\e[1m\e[31m[panic]: %s\e[0m\e[0m\n", s);
	kexit(-1, 1, "panic occured!");
}

int FAKEKBDDVR_getbutton(int fd, char *keytype)
{
	enum KBbutton kn=ORDINARY;
	char buf; int type;
	if(read(fd, &buf, 1)==0) return 0;
	switch (buf) {
	case '\33':
		if(read(fd, &buf, 1)==0) return 0;
		switch (buf) {
		case '[':
			if(read(fd, &buf, 1)==0) return 0;
			switch (buf) {
			case 'A':
				kn=UP;
				break;
			case 'B':
				kn=DOWN;
				break;
			case 'C':
				kn=RIGHT;
				break;
			case 'D':
				kn=LEFT;
				break;
			case 'H':
				kn=HM;
				break;
			case 'F':
				kn=END;
				break;
			case '3':
				if(read(fd, &buf, 1)==0) return 0;
				if(buf=='~') kn=DEL;
				else kn=UNKOWN;
				break;
			default:
				kn=UNKOWN;
			}
			break;
		default:
			kn=UNKOWN;
		}
		break;
	case '\177':
		kn=BACKSPACE;
		break;
	default:
		if(buf>=32 or buf=='\n' or buf=='\t') *keytype=buf;
		else kn=UNKOWN;
	}
	if(kn==UNKOWN) return -1;
	return kn;
}

//copied from printf.c of xv6-riscv
void putc(int fd, char c)
{
	write(fd, &c, 1);
}
void printint(int fd, int xx, int base, int sgn)
{
	static char digits[] = "0123456789ABCDEF";
	char buf[16];
	int i, neg;
	unsigned int x;

	neg = 0;
	if(sgn && xx < 0){
		neg = 1;
		x = -xx;
	} else x = xx;

	i = 0;
	do buf[i++] = digits[x % base];
	while((x /= base) != 0);
	if(neg)
		buf[i++] = '-';

	while(--i >= 0)
		putc(fd, buf[i]);
}
// Print to the given fd. Only understands %d, %x, %s.
void vprintf(int fd, const char *fmt, va_list ap)
{
	char *s;
	int c, i, state = 0;

	for(i = 0; fmt[i]; i++){
		c = fmt[i] & 0xff;
		if(state == 0){
			if(c == '%') state = '%';
			else putc(fd, c);
		} else if(state == '%'){
			if(c == 'd') printint(fd, va_arg(ap, int), 10, 1);
			else if(c == 'l') printint(fd, va_arg(ap, long), 10, 0);
			else if(c == 'x') printint(fd, va_arg(ap, int), 16, 0);
			else if(c == 's') {
				s = va_arg(ap, char*);
				if(s == 0) s = "(null)";
				while(*s != 0) putc(fd, *s++);
			} else if(c == 'c') {
				putc(fd, va_arg(ap, unsigned int));
			} else if(c == '%') {
				putc(fd, c);
			} else {
				// Unknown % sequence.  Print it to draw attention.
				putc(fd, '%');
				putc(fd, c);
			}
			state = 0;
		}
	}
}
void fprintf(int fd, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fd, fmt, ap);
	va_end(ap);
}
void warn(int fd, const char *fmt, ...)
{
	va_list ap;
	fprintf(fd, "\e[1m\e[33m[ksh warning]: ");
	va_start(ap, fmt);
	vprintf(fd, fmt, ap);
	va_end(ap);
	fprintf(fd, "\e[0m\e[0m\n");
}
