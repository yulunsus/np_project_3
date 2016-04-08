#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>


using namespace std;

// #define SERV_TCP_PORT 10268
#define QLEN 5

class Pipe
{
public:
	int ps[2];	// pipes
	bool on;
	
	Pipe()
	{
		ps[0] = ps[1] = -1;
		on = false;
	}
};

class Cmd
{
public:
	int argc;
	char** argv;
	int outType;	// 0: no pipe 
					// 1: |
				 	// 2: !
				 	// 3: >
				 	
	int outLine;	// 0  : cmd1 | cmd2
					// false : cmd1 |n
	string outFile; 
	Cmd()
	{
		argc = 1;
		outType = 0;
		outLine = 0;
	}	
};
/*		function declaration	*/
void welcomeMsg();
void init();
void server_function(int newsockfd);
void err_dump(const char* msg);
void reaper(int sig);

/*		function definition		*/
void welcomeMsg()
{
	cout << "****************************************\n";
	cout << "** Welcome to the information server. **\n";
	cout << "****************************************\n";
}

void init()
{
	setenv("PATH","bin:.",1);	// 1 means overwrite
	chdir("ras");
}

void server_function(int newsockfd)
{
	(void) signal(SIGCHLD,SIG_DFL);
	for ( int i=0; i<3; i++)
	{
		close(i);
		dup(newsockfd);
	}


	int lineNum = -1;
	int childpid;
	string msg;	
	Pipe pipes[1000];
	welcomeMsg();
	init();
	int inputFD = dup(0);	// original input
	int outputFD = dup(1);	// original output
	bool leave = false;
	
	while ( !leave )
	{
		cout <<"% ";
		string line;
		string cmdstr;
		stringstream ssline;
		getline(cin,line);			
		lineNum++;
		lineNum = lineNum % 1000;
		ssline << line;		
		while ( ssline >> cmdstr )
		{	
			if ( cmdstr == "exit")
			{
				leave = true;
				break;
			}
			Cmd cmd;
			stringstream sscmd;
			sscmd << cmdstr << "\n";
			while ( ssline >> cmdstr)
			{
				if ( cmdstr == "|" )
				{
					cmd.outType = 1;
					cmd.outLine = 0;
					break;
				}
				else if ( cmdstr == "!" )
				{
					cmd.outType = 2;
					cmd.outLine = 0;
					break;
				}
				else if ( cmdstr == ">" )
				{
					cmd.outType = 3;
					ssline >> cmd.outFile;
					break;
				}
					
				else if ( cmdstr[0] == '|' || cmdstr[0] =='!')
				{
					stringstream ss;
					ss << cmdstr;
					char c;
					int lineOut;
					ss >> c >> cmd.outLine;
					cmd.outType = ( cmdstr[0] == '|' ) ? 1 : 2;
					break;
				}
				else
				{
					sscmd << cmdstr << '\n';
					cmd.argc++;
				}
			}
			
			cmd.argv = new char* [cmd.argc+1];
			for ( int i=0; i< cmd.argc; i++)
			{
				string arg;
				sscmd >> arg;
				char* cstr = new char[arg.size()+1];
				strcpy(cstr, arg.c_str());
				cmd.argv[i] = cstr;
			}
			cmd.argv[cmd.argc] = NULL;
			// end parsing a cmd
			
			if ( !strcmp(cmd.argv[0],"setenv") )
			{
				setenv(cmd.argv[1],cmd.argv[2],1);
				ssline.str("");
				ssline.clear();
				sscmd.str("");
				sscmd.clear();				
				continue;
			}
			else if ( !strcmp(cmd.argv[0],"printenv") )
			{
				for ( int i=1; i < cmd.argc; i++)
				{
					char* path = getenv(cmd.argv[i]);
					if ( path != NULL)
						cout << cmd.argv[i] << "=" << path << endl;
					else
						cout << "Can't Find " << cmd.argv[1] << endl;
				}
				ssline.str("");
				ssline.clear();
				sscmd.str("");
				sscmd.clear();	
				continue;
			}
			
			int fileFD;
			int filepipe[2];
			Pipe newpipe;
			int target;
			bool only12target;	// only one to target
			
			if ( cmd.outType == 3)	// >
			{
				if ((fileFD = open(cmd.outFile.c_str(),O_WRONLY|O_CREAT)) < 0)
					cerr << "can't open file " << cmd.outFile << " (for >)" << endl;
			}
			if ( cmd.outType != 0 && cmd.outLine == 0 )	// a | b
			{
				if ( pipe(newpipe.ps) < 0 )
					cerr << "can't create pipes." << endl;
				else
					newpipe.on = true;
			}
			else if ( cmd.outType != 0 && cmd.outLine != 0)		// a |n
			{
				target = (lineNum + cmd.outLine) % 1000; 
				
				if ( pipes[target].on == false )
				{
					only12target = true;
					if ( pipe(pipes[target].ps) < 0)
						cerr << "can't create pipes.\r\n";
					
					pipes[target].on = true;
				}
				else
					only12target = false;					
			}
			// fork , close, exec			
			if ( (childpid=fork()) < 0)
				cerr << "cant fork\r\n";
			else if ( childpid == 0) // child
			{		
				// set input: read from pipeOut
				close(0);

				if ( pipes[lineNum].on )
					dup(pipes[lineNum].ps[0]);
				else
					dup(inputFD);
					
				// set output: write to pipeIn
				close(1);
						
				if ( cmd.outType == 0)
					dup(outputFD);
				else if ( cmd.outType == 3)	// >
				{
					dup(fileFD);
					close(fileFD);
				}
				else if ( cmd.outLine == 0 )// a | b
				{				
					dup(newpipe.ps[1]);
					close(newpipe.ps[0]);
					close(newpipe.ps[1]);
				}
				else if ( cmd.outLine != 0)		// a |n
					dup(pipes[target].ps[1]);

				// set error	
				close(2);
				if ( cmd.outType == 2)
					dup(1);
				else
					dup(outputFD);			
					
				// close all the other
				for ( int i=0; i<1000; i++)
				{	
					close(pipes[i].ps[0]);
					close(pipes[i].ps[1]);
				}
				if ( execvp(cmd.argv[0],cmd.argv) < 0)
				{

					close(1);
					dup(outputFD);
					string s1(cmd.argv[0]);
					cout << "Unknown Command: [" << cmd.argv[0] <<"]." << endl;
					close(0);
					close(1);
					close(2);
					exit(-1);
				}
			}
			else	// parent
			{						
				// set input: read from pipeOut
				close(pipes[lineNum].ps[0]);
				close(pipes[lineNum].ps[1]);
				pipes[lineNum].on = false;
					
				// set output: write to pipeIn
				if ( cmd.outType == 3)	// >
					close(fileFD);	
				else if ( cmd.outType != 0 && cmd.outLine == 0 )	// a | b
				{
					pipes[lineNum].ps[0] = newpipe.ps[0];
					pipes[lineNum].ps[1] = newpipe.ps[1];
					pipes[lineNum].on = newpipe.on = true;
				}
		
				int status;
				while ( waitpid(childpid, &status, WNOHANG) != childpid)
					;
				if ( WIFEXITED(status) == -1 ) 
				{
					close(pipes[lineNum].ps[0]);
					close(pipes[lineNum].ps[1]);
					pipes[lineNum].on = false;
					
					if ( cmd.outType != 0 && cmd.outLine != 0 )
					{
						if ( only12target)
						{
							close(pipes[target].ps[0]);
							close(pipes[target].ps[1]);
							pipes[target].on = false;
						}
					}
					ssline.str("");
					ssline.clear();
					continue;
				}
			}
		}
		if (leave)
			break;
	}
}

void err_dump(const char* msg)
{
	write(2,msg,strlen(msg));
	exit(1);
}

void reaper(int sig)
{
//	while(wait3(NULL,WNOHANG,(struct rusage *)0) >= 0)
	wait3(NULL,WNOHANG,(struct rusage *)0);
}

int main(int argc, char** argv)
{
	int sockfd, newsockfd, chpid;
	unsigned int clilen,servlen;
	struct sockaddr_in cli_addr,serv_addr;
	clilen = sizeof(cli_addr);
	servlen = sizeof(serv_addr);

	// Open a TCP socket
	if (( sockfd = socket(AF_INET,SOCK_STREAM,0)) <0)
		err_dump("server: can't open stream socket\n");

	//  Bind out locat address so that the client can send to us
	bzero( (char *)&serv_addr, servlen );
	bzero( (char *)&cli_addr,clilen );	
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons((u_short)atoi(argv[1]));
//	serv_addr.sin_port = htons(SERV_TCP_PORT);
	if ( bind(sockfd,(struct sockaddr *)&serv_addr,servlen) <0)
		err_dump("server: can't bind local address\n");
	listen(sockfd,QLEN);
	
	(void) signal(SIGCHLD,reaper);	
		
	while (1)
	{
		newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
		if ( newsockfd < 0)	err_dump("server: accpt error\n");
		chpid = fork();
		if ( chpid < 0)	err_dump("server: fork error\n");
		else if ( chpid == 0)	// child
		{
			close(sockfd);
			server_function(newsockfd);
			close(newsockfd);
			exit(0);
		}
		else
		{
			close(newsockfd); // parent	
		}
	}
	return 0;
}

