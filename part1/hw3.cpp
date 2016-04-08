#include <iostream>
#include <stdio.h>  
#include <stdlib.h>
#include <string>
#include <sctring> 

#define F_CONNECTING 0
#define F_READING 1
#define F_WRITING 2
#define F_DONE 3
#define BUFFER_SIZE = 2048

using namespace std;

class HOST
{
	public:
	char* host_ip;
	char* host_port;
	char* host_file;
	bool exist;
	int sockfd;
	ifstream batch_instream;
	int status;
	bool is_exit;
	string msg_string;
	HOST(){

		host_ip = new char[strlen("No Server Connection")+1];
		strcpy(host_ip,"No Server Connection");
		exist = false;
		status = F_CONNECTING;
		is_exit = false;
		msg_string = "";
	}	

	void init(char* ip, char* port, char* file){

		delete [] host_ip;
		host_ip = new char[strlen(ip)+1];
		host_ip = ip;
		host_port = new char[strlen(port)+1];
		strcpy(host_port, port);
		host_file = new char[strlen(file)+1];
		strcpy(host_file , file);
		exist = true;
		sockfd = -1;
	}
};

int main(int argc, char* argv[])
{

	HOST* host = new HOST[5];
	printf("Content-type: text/html\r\n");

	char* qeury = getenv("QUERY_STRING");
	char* segments = strtok(query,"&");
	char* ptr;
	for(int i=0; i < 5; i++)
	{
		ptr = strchr(segments,"=");
		if( *(ptr + 1) != '\0'){

			char* ip, port, file;
			ip = ptr + 1;
			segments = strtok(NULL,"&");
			port = ptr + 1;
			segemnts = strtok(NULL,"&");
			file = ptr + 1;

			host[i].init(ip, port, file);

		}
		else{
			// grab 2 "&" after "=""
			segments = strtok(NULL,"&");
			segments = strtok(NULL,"&");
		}
		// grab "&" after each host except the last one
		if(i < 4)
			segments = strtok(NULL,"&");
	}

	printf("<html>");
    printf("<head>");
    printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />");
    printf("<title>Network Programming Homework 3</title>");
    printf("</head>");
    printf("<body bgcolor=#336699>");
    printf("<font face=\"Courier New\" size=2 color=#FFFF99>");
    printf("<table width=\"800\" border=\"1\">");
    printf("<tr>");
    
    for(int i=0; i < 5; i++){
    	if(host[i].exist == true)
    		printf("<td>%s</td>", host[i].host_ip);
    }

    printf("</tr>");
    printf("\n<tr>");
    
    for(int i=0 ; i < 5; i++){
    	if(host[i].exist == true)
    		printf("<td valign=\"top\" id=\"m%d\"></td>", i);
    }
    printf("</tr>");
    printf("\n</table>");
	
    fflush(stdout);

    fd_set rfds; /* readable file descriptors*/
	fd_set wfds; /* writable file descriptors*/
	fd_set rs; /* active file descriptors*/
	fd_set ws; /* active file descriptors*/
    int nfds = FD_SETSIZE;
	FD_ZERO(&rfds); 
	FD_ZERO(&wfds); 
	FD_ZERO(&rs); 
	FD_ZERO(&ws);

    int connection_num = 0;
	for(int i=0; i < 5; i++)
	{
		if(host[i].exist == true){
			
			struct sockaddr_in  client_sin;
    		struct hostent *he;
    		int flag;

			host[i].sockfd = socket(AF_INET, SOCK_STREAM, 0);
			bzero(&client_sin,sizeof(client_sin));
			client_sin.sin_family = AF_INET;

			if((he=gethostbyname(host[i].host_ip)) != NULL)
				client_sin.sin_addr = *((struct in_addr *)he->h_addr);
			else{
				fprintf(stderr, "Please fill the ip field");
				exit(1);
			}
			client_sin.sin_port = htons((u_short)(atoi(host[i].host_port)));

			flag = fcntl(host[i].sockfd, F_GETFL, 0);
			fcntl(host[i].sockfd, F_SETFL, flag_0 | O_NONBLOCK);

			if(connect(host[i].sockfd, (struct sockaddr *)&client_sin, sizeof(client_sin)) == -1){
				if(errno != EINPROGRESS) return -1;
			}

			if(host[i].sockfd != -1){

				connection_num++;
				FD_SET(host[i].sockfd, &rs);
				FD_SET(host[i].sockfd, &ws);
				host[i].batch_instream.open(host[i].host_file,ios::in);
			}
		}
	}
	
	

	
	

	while(connection_num > 0){

		memcpy(&rfds, &rs, sizeof(rfds));
		memcpy(&wfds, &ws, sizeof(wfds));
		if(select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0 ) errexit();


		for(int i=0; i < 5; i++)
		{
			if(FD_ISSET(host[i].sockfd, &rfds) || FD_ISSET(host[i].sockfd, &wfds))
			{
				if(host[i].status == F_CONNECTING){
					
					int error;
					socklen_t n = sizeof(int);
					if (getsockopt(host[i].sockfd, SOL_SOCKET, SO_ERROR, &error, &n) < 0 || error != 0) {
						// non-blocking connect failed
						return (-1);
					}
					host[i].status = F_WRITING;
					FD_CLR(host[i].sockfd, &rs);
				}
				else if(host[i].status == F_WRITING && FD_ISSET(host[i].sockfd, &wfds)){

					if(host[i].is_exit == true){
						FD_CLR(host[i].sockfd, &ws);
						host[i].status = F_READING;    // go reading response from server
						FD_SET(host[i].sockfd, &rs);
						continue;
					}
					//read cmd from batch file
					string line;
					getline(host[i].batch_instream,line);
					unsigned r;
					while((r = line.find('\r')) != -1){
						line.erase(r,1);
					}
					printf("<script>document.all['m%d'].innerHTML += \"<b>%s</b><br>\";</script>",i, line.c_str());
					fflush(stdout);

					if(line == "exit")
						host[i].is_exit = true;

					line = line + "\n";
					write(host[i].sockfd, line.c_str(), line.size());
					FD_CLR(host[i].sockfd, &ws);
					host[i].status = F_READING;   // go reading response from server
					FD_SET(host[i].sockfd, &rs);
				}
				else if(host[i].status == F_READING && FD_ISSET(host[i].sockfd, &rfds)){

					char msg_buf[BUFFER_SIZE];
					int read_status = 0;
					// read from server
					while(true)
					{

						if(host[i].msg_string.size() > 0)
						{
							unsigned r_pos, endline_pos, db_quote_pos = 0;
							bool has_cmd_to_write = false;

							while((r = host[i].msg_string.find('\r')) != -1)
								host[i].msg_string.eras(r_pos,1);

							while(host[i].msg_string.size() != 0 && (endline_pos = host[i].msg_string.find('\n') != -1){

								if(host[i].msg_string[0] == '%')
									has_cmd_to_write = true;
								string msg_without_endline = host[i].msg_string.substr(0, endline_pos);

								while((db_quote_pos = msg_without_endline.find('\"', db_quote_pos)) != -1){
										
										msg_without_endline.insert(db_quote_pos, 1, '\\');
								}
								printf("<script>document.all['m%d'].innerHTML += \"<b>%s</b><br>\";</script>",i, msg_without_endline.c_str());
								fflush(stdout);
								//erase parsed string
								host[i].msg_string.erase(0, endline_pos);
							}

							if(has_cmd_to_write == true){

								FD_CLR(host[i].sockfd, &rs);
								host[i].status = F_WRITING;
								FD_SET(host[i].sockfd, &ws);
								break;
							}

							if((host[i].msg_string.size() !=0) && (host[i].msg_string[0] == '%')){
								
								string sub = host[i].msg_string.substr(0,2);
								host[i].msg_string.erase(0,2);
								printf("<script>document.all['m%d'].innerHTML += \"<b>%s</b><br>\";</script>",i, sub.c_str());
								fflush(stdout);
								FD_CLR(host[i].sockfd, &rs);
								host[i].status = F_WRITING;
								FD_SET(host[i].sockfd, &ws);
								break;
							}
						}

						memset(msg_buf, 0, BUFFER_SIZE);
						read_status = read(host[i].sockfd, msg_buf, BUFFER_SIZE);
						if(read_status == -1 && errno == EAGAIN)
							break;   // break to try again
						else if(read_status == -1 && errno != EAGAIN)
							exit(1);   // no need to try anymore
						else if(read_status == 0){   // end of file
							
							// close connection
							FD_CLR(host[i].sockfd, &arfds);
							host[i].status = F_DONE;
							connections--;
							break;
						}
						host[i].msg_string.append(msg_buf);	
					}
				}
			}


		}

	}

    



	return 0;
    
	
}