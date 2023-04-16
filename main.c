#include <myheaders.h>
#include <util.h>
#define CMD_MAXN 100
#define PATH_MAX 1024
#define MAXARGS 20

enum { EXEC = 1, REDIR, PIPE, BACK };
char buf[CMD_MAXN], token[CMD_MAXN][CMD_MAXN];
int cnt=0, cursor=0, historyfd, pid;
int tokenlinecnt=0, back=0, line=0, lines=__INT_MAX__;
pid_t childpid;
struct stat statbuf;

// copied from sh.c of xv6-riscv
struct cmd {
	int type;
};
struct execcmd {
	int type;
	char *argv[MAXARGS];
};
struct redircmd {
	int type;
	struct cmd *cmd;
	char *file;
	int mode;
	int fd;
};
struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};
struct backcmd {
	int type;
	struct cmd *cmd;
};
// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
	if(cmd == 0) kexit(1, 0, NULL);
	int p[2];
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	switch(cmd->type){
		default:
			panic("unkown cmd type!");
		case EXEC:
			ecmd = (struct execcmd*)cmd;
			if(ecmd->argv[0] == 0)
				kexit(2, 1, "argument too less\n");
			execvp(ecmd->argv[0], ecmd->argv);
			warn(2, "exec %s failed: No such executable file\n", ecmd->argv[0]);
			break;
		case REDIR:
			rcmd = (struct redircmd*)cmd;
			close(rcmd->fd);
			if(open(rcmd->file, rcmd->mode, 0666) < 0){
				warn(2, "open %s failed : No such file or Permission denied\n", rcmd->file);
				kexit(3, 0, NULL);
			}
			runcmd(rcmd->cmd);
			break;
		case PIPE:
			pcmd = (struct pipecmd*)cmd;
			if(pipe(p) < 0) panic("can not create pipe!");
			if((childpid=fork1()) == 0){
				close(1);
				dup(p[1]);
				close(p[0]);
				close(p[1]);
				runcmd(pcmd->left);
			}
			if((childpid=fork1()) == 0){
				close(0);
				dup(p[0]);
				close(p[0]);
				close(p[1]);
				runcmd(pcmd->right);
			}
			close(p[0]);
			close(p[1]);
			wait(0);
			wait(0);childpid=0;
			break;
		case BACK:
			bcmd = (struct backcmd*)cmd;
			if(fork1() == 0) runcmd(bcmd->cmd);
			childpid=0;
	}
	kexit(74751, 0, NULL);
}

char delimiter[]=" |<>";
int peek(char key)
{
	for(int i=1; i<=4; ++i) {
		if(delimiter[i-1]==key) return i;
	}return 0;
}
char* trim(char *s, char **ends)
{
	while (*s==' ') ++s;
	while (*--*ends==' ') ;
	if(**ends=='&') {
		back=1;
		while (*--*ends==' ') ;
	}
	*++*ends=0;
	return s;
}
void processdelimiter(char *s, char **ends)
{
	char afterprocess[CMD_MAXN], *start=s;
	int index=0, state=0, quotes=0;
	while (s<*ends) {
		if(quotes) {
			if(*s=='\"') quotes=0;
			afterprocess[index++]=*s;
			++s; continue;
		}
		if(!quotes and *s=='\"') {
			if(state==1) afterprocess[index++]=' ', state=0;
			quotes=1; afterprocess[index++]=*s; ++s;
			continue;
		}
		if(peek(*s)) {
			if(*s==' ') {
				if(state==0) state=1;
			}else afterprocess[index++]=*s, state=2;
		}else {
			if(state==1) afterprocess[index++]=' ';
			afterprocess[index++]=*s, state=0;
		}
		++s;
	}
	afterprocess[index]=0;
	strcpy(start, afterprocess);
	*ends=start+index;
}
char* gettoken(char *s, const char *ends)
{
	int quotes=0;
	char *start=s;
	while (1) {
		if(quotes) {
			if(s>=ends) {
				warn(2, "syntax error! lose paired quotation mark\n");
				return NULL;
			}
			if(*s=='\"') quotes=0;
			++s; continue;
		}
		if(!quotes and *s=='\"') {quotes=1; ++s; continue;}
		if(peek(*s) or s>=ends) break;
		++s;
	}
	if(s!=start) --s;
	return s;
}
struct cmd* buildexec(int line)
{
	struct execcmd *cmd=NULL;
	int len= strlen(token[line]), num=0, last=0, quotes=0;
	if(len) {
		cmd= (struct execcmd*)malloc(sizeof(struct execcmd));
		memset(cmd, 0, sizeof(*cmd));
		cmd->type=EXEC;
		for(int i=0; i<len; ++i) {
			if(quotes) {
				if(token[line][i]=='\"') {
					quotes=0;
					cmd->argv[num]= (char *)malloc(CMD_MAXN);
					memcpy(cmd->argv[num], token[line]+last, i-last);
					cmd->argv[num][i-last]=0;
					if(token[line][i+1]==' ') ++i;
					last=i+1, ++num;
				}
				continue;
			}
			if(!quotes and token[line][i]=='\"') {
				last=i+1;
				quotes=1;
				continue;
			}
			if(token[line][i]==' ' or i+1==len) {
				cmd->argv[num]= (char *)malloc(CMD_MAXN);
				memcpy(cmd->argv[num], token[line]+last, i-last+(i+1==len));
				for(int j=0; j<i-last+(i+1==len); ++j)	// chop '\n'
					if(cmd->argv[num][j]=='\n') cmd->argv[num][j]=0;
				cmd->argv[num][i-last+(i+1==len)]=0;
				last=i+1, ++num;
			}
		}
	}
	return (struct cmd*)cmd;
}
struct cmd* buildredir(int line)
{
	struct cmd *head, *temp;
	// build exec
	head=temp= buildexec(line-1);
	// build redirect
	int len= strlen(token[line]), last=0;
	if(len) {
		for(int i=0; i<len; ++i) {
			if(token[line][i]==' ' or i+1==len) {
				head= (struct cmd*)malloc(sizeof(struct redircmd));
				memset(head, 0, sizeof(*head));
				((struct redircmd*)head)->type=REDIR;
				((struct redircmd*)head)->file=(char *) malloc(CMD_MAXN);
				memcpy(((struct redircmd*)head)->file, token[line]+last+1, i-last-(i+1!=len));
				for(int j=0; j<i-last-(i+1!=len); ++j)	// chop '\n'
					if(((struct redircmd*)head)->file[j]=='\n')
						((struct redircmd*)head)->file[j]=0;
				((struct redircmd*)head)->file[i-last-(i+1!=len)]=0;
				if(token[line][last]=='<') {
					((struct redircmd *) head)->fd=0;
					((struct redircmd *) head)->mode=O_RDONLY;
				}else if(token[line][last]=='>') {
					((struct redircmd *) head)->fd=1;
					((struct redircmd *) head)->mode=O_WRONLY|O_CREAT|O_TRUNC;
				}
				((struct redircmd*)head)->cmd=temp;
				temp=head;
				last=i+1;
			}
		}
	}
	return head;
}
struct cmd* buildsyntaxtree()
{
	struct cmd *root= NULL;
	// pipe exist
	for(int i=tokenlinecnt-3; i>=2; i-=3) {
		struct pipecmd *temp= (struct pipecmd*)malloc(sizeof(struct pipecmd));
		temp->type=PIPE;
		// build the left side of pipe
		temp->left=buildredir(i-1);
		// build the right side of pipe
		if(root==NULL) temp->right=buildredir(i+2);
		else temp->right=root;
		root=(struct cmd *)temp;
	}
	// no pipe
	if(tokenlinecnt<3) root= buildredir(1);
	if(back) {
		struct backcmd *temp= (struct backcmd*)malloc(sizeof(struct backcmd));
		temp->type=BACK;
		temp->cmd=root;
		root=(struct cmd *)temp;
	}
	return root;
}
struct cmd* parsecmd(char *s, char *ends)
{
	char *temp;
	tokenlinecnt=0;
	int tokenlineoffset, len, lastisredir=0;
	while ((temp= gettoken(s, ends))<ends) {
		if(temp==NULL) return NULL;
		tokenlineoffset=0, len=temp-s+1;
		if(*s=='|') tokenlineoffset=-1;
		if(lastisredir==1) {
			tokenlineoffset=1;
			lastisredir=0;
		}
		if(*s=='<' or *s=='>') {
			tokenlineoffset=1;
			lastisredir=1;
			int final=strlen(token[tokenlinecnt+tokenlineoffset]);
			if(final) token[tokenlinecnt+tokenlineoffset][final]=' ';
		}
		int final= strlen(token[tokenlinecnt+tokenlineoffset]);
		memcpy(token[tokenlinecnt+tokenlineoffset]+final, s, len);
		s=temp+1;
		if(*s=='|') tokenlinecnt+=3;
	}
	tokenlinecnt+=2;
	return buildsyntaxtree();
}

void my_signal_handler()
{
	if(childpid!=0) {	//only the parent process might burst into the if-branch below
		fprintf(ksh_write_fd, "\nchild process %d had been terminated\n", childpid);
		kill(childpid, SIGTERM);
		childpid=0;
	}
}
void write_to_history()
{
	assert(historyfd==3);	// ensure that the operation "open", "close" is paired
	lseek(historyfd, 0, SEEK_END);
	write(historyfd, buf, CMD_MAXN);
	write(historyfd, "\n", 1);
}
int read_from_history()
{
	int resnum=0;
	assert(historyfd==3);	// ensure that the operation "open", "close" is paired
	int offset=lseek(historyfd, (-CMD_MAXN-1)*line, SEEK_END);
	if(offset<=0) {
		resnum=-1;
		lines=line;
	}
	read(historyfd, buf, CMD_MAXN);
	return resnum;
}
int getcmd()
{
	fstat(0, &statbuf);
	if(!S_ISFIFO(statbuf.st_mode)) {
		char dirmsg[PATH_MAX];
		if(getcwd(dirmsg, PATH_MAX)==NULL) panic("current directory path limits exceed!");
		fprintf(ksh_write_fd, "\e[35mk %d$ %s> \e[0m", getpid(), dirmsg);
	}
	
	int type, len; char keytype, tempbuff[CMD_MAXN*3];
	memset(buf, '\0', CMD_MAXN); cnt=cursor=0;
	while(type=FAKEKBDDVR_getbutton(0, &keytype)) {
		if(type==-1) panic("unkown keytype.");
		switch (type) {
		case ORDINARY:
			if(keytype=='\n' and !S_ISFIFO(statbuf.st_mode)) {
				write(ksh_write_fd, "\n", 1);
				return 0;
			}else if(keytype!='\t') {
				if(cnt<CMD_MAXN) {
					for(int i=cnt; i>cursor; --i)
						buf[i]=buf[i-1];
					buf[cursor]=keytype;
					++cursor; ++cnt;
					if(!S_ISFIFO(statbuf.st_mode)) {
						write(ksh_write_fd, buf+cursor-1, cnt-cursor+1);
						memset(tempbuff, '\b', cnt-cursor);
						write(ksh_write_fd, tempbuff, cnt-cursor);
					}
				}else panic("cmd line limits exceed!");
			}
			break;
		case BACKSPACE:
			if(cursor>0) {
				--cursor; --cnt;
				for(int i=cursor; i<=cnt; ++i)
					buf[i]=buf[i+1];
				write(ksh_write_fd, "\b", 1);
				write(ksh_write_fd, buf+cursor, cnt-cursor);
				tempbuff[0]=' ';
				memset(tempbuff+1, '\b', cnt-cursor+1);
				write(ksh_write_fd, tempbuff, cnt-cursor+2);
			}else write(ksh_write_fd, "\7", 1);
			break;
		case UP:
			if(line<lines) {
				++line;
				memset(tempbuff, '\b', cursor);
				memset(tempbuff+cursor, ' ', cnt);
				memset(tempbuff+(cursor+cnt), '\b', cnt);
				write(ksh_write_fd, tempbuff, (cnt<<1)+cursor);
				read_from_history();
				len=strlen(buf);
				write(ksh_write_fd, buf, len);
				cursor=cnt=len;
			}else write(ksh_write_fd, "\7", 1);
			break;
		case DOWN:
			if(line>0) {
				--line;
				memset(tempbuff, '\b', cursor);
				memset(tempbuff+cursor, ' ', cnt);
				memset(tempbuff+(cursor+cnt), '\b', cnt);
				write(ksh_write_fd, tempbuff, (cnt<<1)+cursor);
				cursor=cnt=0;
				if(line) {
					read_from_history();
					len=strlen(buf);
					write(ksh_write_fd, buf, len);
					cursor=cnt=len;
				}
			}else write(ksh_write_fd, "\7", 1);
			break;
		case HM:
			memset(tempbuff, '\b', cursor);
			write(ksh_write_fd, tempbuff, cursor);
			cursor=0;
			break;
		case END:
			for(int i=0; i<(cnt-cursor)*3; i+=3)
				tempbuff[i]='\e', tempbuff[i+1]='[', tempbuff[i+2]='C';
			write(ksh_write_fd, tempbuff, (cnt-cursor)*3);
			cursor=cnt;
			break;
		case DEL:
			if(cursor<cnt) {
				for(int i=cursor; i<cnt; ++i)
					buf[i]=buf[i+1];
				--cnt;
				write(ksh_write_fd, buf+cursor, cnt-cursor);
				tempbuff[0]=' ';
				memset(tempbuff+1, '\b', cnt-cursor+1);
				write(ksh_write_fd, tempbuff, cnt-cursor+2);
			}else write(ksh_write_fd, "\7", 1);
			break;
		case RIGHT:
			if(cursor<cnt) {
				++cursor;
				write(ksh_write_fd, "\e[C", 3);
			}else write(ksh_write_fd, "\7", 1);
			break;
		case LEFT:
			if(cursor>0) {
				--cursor;
				write(ksh_write_fd, "\b", 1);
			}else write(ksh_write_fd, "\7", 1);
		}
	}
	return (!cnt)?-1:cnt;
}

int main()
{
	pid=getpid();
	assert((long)signal(SIGINT, my_signal_handler)!=-1);
	historyfd=open(".ksh_history", O_RDWR | O_CREAT, 0666);
	init_tty_mode();
	fstat(0, &statbuf);
	// read and run input cmd
	while(getcmd()>=0) {
		if(!S_ISFIFO(statbuf.st_mode) and cnt) {
			write_to_history();
			line=0, lines=__INT_MAX__;
		}
		char *s=buf, *ends=s+cnt;
		back=0;
		s=trim(s, &ends);
		processdelimiter(s, &ends);
		if(s[0] == 'c' and s[1] == 'd' and s[2] == ' '){
			// chdir must be called by the parent, not the child.
			if(chdir(s+3)<0)
				warn(2, "cannot cd to %s: no such a directory\n", s+3);
			continue;
		}
		if(s[0] == 'e' and s[1] == 'x' and s[2] == 'i' and s[3] == 't')
			kexit(0, 1, "ksh exit with status");
		if((childpid=fork1()) == 0) {
			close(historyfd);
			runcmd(parsecmd(s, ends));
		}
		wait(0);childpid=0;
		init_tty_mode();
	}
	reset_tty_mode();
}
