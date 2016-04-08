#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace std;

#define BUFFER_SIZE 1024

void sig_fork(int signo)
{
    pid_t pid;
    int stat;
    pid=waitpid(0,&stat,WNOHANG);
    
    return;
}
void reaper(int sig);
void server_function(int sockfd);
void write_msg(int sockfd,const char* msg);

int main(int argc, char* argv[])
{

	if(argc != 2){

		cout << "Please enter the port number!" << endl;
		exit(1);
	}

	int sockfd , client_sockfd , clilen, childpid;
	struct sockaddr_in serv_addr, cli_addr;
	char* port = argv[1];
	int port_num = atoi(port);  

	//create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){

		cout << "Could not create socket" << endl;
	}
	puts("Socket created");

	// prepare the serv_addr_in structure

	bzero((char *)&serv_addr, sizeof(serv_addr));
	bzero((char *)&cli_addr, sizeof(cli_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port_num);
	
	// bind
	if(bind(sockfd,(struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");

	// Listen
	listen(sockfd , 5);
	
	// accept incoming connection
	puts("Waiting for incoming connections...");
	clilen = sizeof(struct sockaddr_in);

	// prevent child zombie process
	(void) signal(SIGCHLD, reaper);

	while(true)
	{
		client_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t*) &clilen);
		if(client_sockfd < 0){

			perror("accept failed");
			return 1;
		}
		puts("Connection accepted.");
		
		//Receive a message from client
		childpid = fork();

		if(childpid < 0)
			perror("server: fork error");
		else if(childpid == 0){

    		close(sockfd);
			server_function(client_sockfd);  
    		close(client_sockfd);
    		
    		exit(0);
		}
		else
			close(client_sockfd);
	}	

	return 0;
}


void server_function(int sockfd)
{
	int original_output = dup(1);

	char* buffer = new char[BUFFER_SIZE];
			
	string request;
	request = "";
	int Connection_pos;

	// we only need the information before Connection status is shown
	while((Connection_pos = request.find("Connection")) == -1){

		int read_status;
		memset(buffer, 0, BUFFER_SIZE);
		read_status = read(sockfd, buffer, BUFFER_SIZE);
		request.append(buffer);
	}
	
	char* request_cstr = new char[BUFFER_SIZE];
	strncpy(request_cstr, request.c_str(), Connection_pos);

	char* request_type = strtok(request_cstr, " ");

	if(strcmp(request_type,"GET")){

		cout << "Sockfd(" << sockfd << "): Not GET request !" << endl;
		return;
	}

	int endline_pos = request.find('\n');
	string first_line = request.substr(0, endline_pos);
	request.erase(0,endline_pos+1);

	endline_pos = request.find('\r');
	string second_line = request.substr(0,endline_pos);
	
	int second_space_pos = first_line.rfind(' ');
	string after_ip = first_line.substr(4,second_space_pos-5+1);
	request.erase(0,endline_pos+1);
	endline_pos = second_line.find('\r');
	string ip_port_str = second_line.substr(6, endline_pos-6+1);

	string query_string = after_ip;
	

	int question_mark_pos = query_string.find('?');
	bool no_question_mark = false;
	if(question_mark_pos == -1)
		no_question_mark = true;

	string target_filename;


	if(no_question_mark == false){   // ? exist
		
		target_filename = query_string.substr(1, question_mark_pos-1);
		query_string = query_string.substr(question_mark_pos+1, query_string.size()-1);
	}
	else{

		target_filename = query_string.substr(1, query_string.size()-1);
		query_string = "";
	}

	cout << "target_filename = " << target_filename << "novaluehere"<< endl;
	cout << "query_string = " << query_string << "novaluehere" << endl;
	// Set environmental variables
	setenv("QUERY_STRING",query_string.c_str(),1);
	setenv("CONTENT_LENGTH","6653",1);
	setenv("REQUEST_METHOD","GET",1);
	setenv("SCRIPT_NAME", after_ip.c_str(), 1);
	setenv("REMOTE_HOST", ip_port_str.c_str(), 1);
	setenv("REMOTE_ADDR", ip_port_str.c_str(), 1);
	setenv("AUTH_TYPE", "auth_type", 1);
	setenv("REMOTE_USER", "remote_user", 1);
	setenv("REMOTE_IDENT", "remote_ident", 1);

	write_msg(sockfd, "HTTP 200 OK\n");
	fflush(stdout);

	if(strstr(target_filename.c_str(), ".cgi") != NULL){

		dup2(sockfd, 1);
		dup2(sockfd, 2);
		execl(target_filename.c_str(), target_filename.c_str(), NULL);
	}
	else{
		
		write_msg(sockfd, "Content-Type: text/html\n\n\n");
		
		ifstream infs;
		infs.open(target_filename.c_str(), ios::in);
		
		string to_write;
		to_write = "";
		while(getline(infs, to_write))
		{
			write_msg(sockfd, to_write.c_str());
			to_write = "";
		}
	}
	fflush(stdout);
}

void reaper(int sig)
{
	wait3(NULL,WNOHANG,(struct rusage *)0);
}

void write_msg(int sockfd, const char* msg)
{
	char* buffer_write = new char[strlen(msg)];
	memset(buffer_write, 0, strlen(msg));
	strcpy(buffer_write, msg);
	int write_result, write_index = 0;
	int num_to_write = strlen(msg);

	while(write_index != num_to_write){

		write_result = write(sockfd, buffer_write + write_index, num_to_write);
		if(write_result != -1)
			write_index += write_result;
	}
	delete [] buffer_write;

}