#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <list>
using namespace std;

#include "resource.h"

#define bzero(a, b) memset(a, 0x0, b)
#define SERVER_PORT 6633
#define BufSize 2048
#define F_CONNECTING 0
#define F_READING 1
#define F_COMMAND 2
#define F_DONE 3
#define WM_SOCKET_NOTIFY (WM_USER + 1)

//=================================================================
// Class Declarations
//=================================================================
class Host
{
public:
	char* ip;
	char* port;
	char* batch;
	bool is_on;
	bool is_done;
	SOCKET sockfd;
	ifstream batch_stream;
	string sockStr;
	int status;
	
	Host()
	{
		init();
	}
	void init()
	{
		if ( ip != NULL)
			delete[] ip;
		ip = new char[strlen("No Server")+1];
		strcpy(ip,"No Server");
		is_on = false;
		status = F_CONNECTING;
		is_done = false;	
	}

	void init(char* in_ip, char* in_port,char* in_batch)
	{
		if ( ip != NULL)
			delete[] ip;
		ip = new char[strlen(in_ip)+1];
		strcpy(ip,in_ip);
		port = new char[strlen(in_port)+1];
		strcpy(port,in_port);
		batch = new char[strlen(in_batch)+1];
		strcpy(batch,in_batch);
		is_on = true;
		sockStr = "";
		is_done = false;
		
	}
};


BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);

list<SOCKET> Socks;
Host host[5];
SOCKET cgisock;
int connection_num;

void write_msg(SOCKET sockfd, const char* msg);
void http_server(SOCKET sockfd, HWND hwnd, HWND hwndEdit);
void read_host(int i, HWND hwnd, HWND hwndEdit);
void read_batch(int i, HWND hwnd, HWND hwndEdit);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static HWND hwndEdit;
	static SOCKET msock, ssock;
	static struct sockaddr_in sa;

	int err;
	bool host_reading;

	switch(Message) 
	{
		case WM_INITDIALOG:
			hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_LISTEN:

					WSAStartup(MAKEWORD(2, 0), &wsaData);

					//create master socket
					msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					if( msock == INVALID_SOCKET ) {
						EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
						WSACleanup();
						return TRUE;
					}

					err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ );

					if ( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
						closesocket(msock);
						WSACleanup();
						return TRUE;
					}

					//fill the address info about server
					sa.sin_family		= AF_INET;
					sa.sin_port			= htons(SERVER_PORT);
					sa.sin_addr.s_addr	= INADDR_ANY;

					//bind socket
					err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
						WSACleanup();
						return FALSE;
					}

					err = listen(msock, 2);
		
					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
						WSACleanup();
						return FALSE;
					}
					else {
						EditPrintf(hwndEdit, TEXT("=== Server START(%d) ===\r\n"),SERVER_PORT);
					}

					break;
				case ID_EXIT:
					EndDialog(hwnd, 0);
					break;
			};
			break;

		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;

		case WM_SOCKET_NOTIFY:
			switch( WSAGETSELECTEVENT(lParam) )
			{
				case FD_ACCEPT:
					ssock = accept(msock, NULL, NULL);
					Socks.push_back(ssock);
					err = WSAAsyncSelect(ssock, hwnd, WM_SOCKET_NOTIFY, FD_READ );				
					EditPrintf(hwndEdit, TEXT("=== Accept new client (sockfd: %d) ===\r\n"), ssock);
					break;
				case FD_READ:
					host_reading = false;
					for(int i=0; i<5; i++){

						if(host[i].is_on && host[i].sockfd == wParam){
							host_reading = true;
							read_host(i,hwnd,hwndEdit);
							break;
						}
					}
					if (!host_reading)
						http_server(wParam, hwnd, hwndEdit);
					break;
				case FD_WRITE:
					for(int i=0; i<5; i++){

						if(host[i].sockfd == wParam && (!host[i].is_done)){
							read_batch(i,hwnd,hwndEdit);
							break;
						}
					}
					break;
				case FD_CLOSE:
					break;
			};
			break;
		
		default:
			return FALSE;


	};

	return TRUE;
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
     TCHAR   szBuffer [1024] ;
     va_list pArgList ;

     va_start (pArgList, szFormat) ;
     wvsprintf (szBuffer, szFormat, pArgList) ;
     va_end (pArgList) ;

     SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
     SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
     SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
	 return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0); 
}

void http_server(SOCKET sockfd, HWND hwnd, HWND hwndEdit)
{
	WSAAsyncSelect(sockfd, hwnd, WM_SOCKET_NOTIFY,0);
	char *request;

	string string_buffer;
	int read_status;
	
	char buf[BufSize];
	memset(buf,0,BufSize);
	read_status = recv(sockfd, buf, BufSize,0);
	string_buffer.assign(buf);

	int pos = string_buffer.find("Connection:");
	if(pos == -1)
		return;

	EditPrintf(hwndEdit, TEXT("---------- connected (%d) ----------\r\n"), sockfd);

	request = new char[pos+1];
	memset(request, 0, pos+1);
	strncpy(request, string_buffer.c_str(), pos);

	char* method = strtok(request, " ");
	char* script = strtok(NULL, " ");
	script = script+1;

	if(strcmp(method,"GET")){

		EditPrintf(hwndEdit, TEXT("not GET type \r\n"), sockfd);
		return;
	}

	char* file_name = strtok(script, " ?");
	
	char* query = strtok(NULL, " ");
	EditPrintf(hwndEdit, TEXT("file_name = %s\r\n"), file_name);
	EditPrintf(hwndEdit, TEXT("query = %s\r\n"), query);

	write_msg(sockfd,"HTTP 200 OK\n");

	if(strstr(file_name, ".cgi") != NULL){

		cgisock = sockfd;
		char* tmp = strtok(query,"&");
		char* ptr;
		char* temp_ip;
		char* temp_port;
		char* temp_batch;

		for(int i=0; i<5; i++){

			ptr = strchr(tmp, '=');
			if(*(ptr+1) != '\0'){

				temp_ip = ptr+1;
				tmp = strtok(NULL,"&");
				ptr = strchr(tmp, '=');
				temp_port = ptr+1;
				tmp = strtok(NULL,"&");
				ptr = strchr(tmp, '=');
				temp_batch = ptr+1;
				host[i].init(temp_ip,temp_port,temp_batch);
			}
			else{

				host[i].init();
				tmp = strtok(NULL,"&");
				tmp = strtok(NULL,"&");
			}
			if (i != 4)
				tmp = strtok(NULL,"&");
		}

		write_msg(cgisock,"Content-type: text/html\n\n");
		write_msg(cgisock,"<html>");
		write_msg(cgisock,"<head>");
		write_msg(cgisock,"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />");
		write_msg(cgisock,"<title>Network Programming Homework 3</title>");
		write_msg(cgisock,"</head>");
		write_msg(cgisock,"<style type='text/css'> xmp {display: inline;} </style>");	
		write_msg(cgisock,"<body bgcolor=#336699>");
		write_msg(cgisock,"<font face=\"Courier New\" size=2 color=#FFFF99>");

		for(int i=0; i<5; i++){

			memset(buf,0,BufSize);
			sprintf(buf,"<xmp>Host%d:    ip=%s    port=%s    batch=%s</xmp><br>",i+1,host[i].ip,host[i].port,host[i].batch);
			write_msg(cgisock,buf);
		}

		write_msg(cgisock,"<table width=\"800\" border=\"1\">");
		write_msg(cgisock,"<tr>");

		for(int i=0; i<5; i++){

			memset(buf,0,BufSize);
			sprintf(buf,"<td><xmp>%s</xmp></td>",host[i].ip);
			write_msg(cgisock,buf);
		}

		write_msg(cgisock,"</tr>");

		write_msg(cgisock,"<tr>");
		write_msg(cgisock,"<td valign=\"top\" id=\"m0\"></td><td valign=\"top\" id=\"m1\"></td><td valign=\"top\" id=\"m2\"><td valign=\"top\" id=\"m3\"><td valign=\"top\" id=\"m4\"></td></tr>");
		write_msg(cgisock,"</table>");

		connection_num = 0;
		for(int i=0; i<5; i++)
		{
			if(!host[i].is_on){
				memset(buf,0,BufSize);

				sprintf(buf,"<script>document.all['m%d'].innerHTML += \"<br>\";</script>",i);
				write_msg(cgisock,buf);
			}
			else{

				struct sockaddr_in client;
				struct hostent *he;
				unsigned int clilen = sizeof(client);

				host[i].sockfd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);

				if( host[i].sockfd == INVALID_SOCKET)
					EditPrintf(hwndEdit, TEXT("CGI hosts: socket() Error"));
	 			bzero(&client,clilen);
	 			client.sin_family = AF_INET;

				if((he = gethostbyname(host[i].ip)) != NULL)
					client.sin_addr = *((struct in_addr *)he->h_addr);
				else
					client.sin_addr.s_addr = inet_addr(host[i].ip);

				client.sin_port = htons((u_short) ((atoi) (host[i].port)) );
		
				connect(host[i].sockfd, (struct sockaddr *)&client, clilen);
				if(host[i].sockfd != -1){

					host[i].batch_stream.open(host[i].batch,ios::in);
					WSAAsyncSelect(host[i].sockfd, hwnd, WM_SOCKET_NOTIFY, FD_READ );
					connection_num++;
					EditPrintf(hwndEdit, TEXT("host[%d] (%d) connected,wait READ\r\n"),i,host[i].sockfd);
				}
			}
		}
	}
	else{

		write_msg(sockfd,"Content-Type: text/html\n\n\n");
		ifstream script_stream;
		script_stream.open(file_name, ios::in);
		string_buffer.assign("");

		while(getline(script_stream,string_buffer))
		{
			string_buffer = string_buffer + "\n";
			write_msg(sockfd, string_buffer.c_str());
			string_buffer.assign("");
		}
		closesocket(sockfd);
    }
	EditPrintf(hwndEdit, TEXT("---------- done           (%d) ----------\r\n"),sockfd);
}

void write_msg(SOCKET sockfd, const char* msg)
{
	char* write_buffer = new char[strlen(msg)];
	memset(write_buffer, 0, strlen(msg));
	strcpy(write_buffer, msg);

	int write_status;
	int write_idx = 0;
	int total_write = strlen(msg);

	while(write_idx != total_write){

		write_status = send(sockfd,write_buffer+write_idx,total_write-write_idx,0);
		if( write_status != -1)
			write_idx += write_status;
	}
}

void read_host(int i, HWND hwnd, HWND hwndEdit)
{
	int read_status = 0;
	// read from host
	while(true)
	{
		if (host[i].sockStr.size() !=0){

			unsigned r;
			while((r = host[i].sockStr.find('\r')) != -1)
				host[i].sockStr.erase(r,1);
			unsigned end;
			bool exec_cmd = false;
			while((host[i].sockStr.size() !=0) && ((end = host[i].sockStr.find('\n')) != -1)){

				if (host[i].sockStr[0] == '%')
					exec_cmd = true;
				string line = host[i].sockStr.substr(0,end);// no \n
				unsigned p = -2;
				while((p = line.find('\"',p+2)) != -1)
					line.insert(p,1,'\\');
				char buf[BufSize];
				memset(buf,0,BufSize);
				sprintf(buf,"<script>document.all['m%d'].innerHTML += \"<xmp>%s</xmp><br>\";</script>\n",i,line.c_str());
				write_msg(cgisock,buf);
				host[i].sockStr.erase(0,end+1);
			}
			if(exec_cmd){

				host[i].status = F_COMMAND;
				WSAAsyncSelect(host[i].sockfd, hwnd, WM_SOCKET_NOTIFY,FD_WRITE);
				break;
			}
			if((host[i].sockStr.size() !=0) && (host[i].sockStr[0] == '%')){

				string sub = host[i].sockStr.substr(0,2);
				host[i].sockStr.erase(0,2);
				char buf[BufSize];
				memset(buf,0,BufSize);
				sprintf(buf,"<script>document.all['m%d'].innerHTML += \"<xmp>%s</xmp>\";</script>\n", i,sub.c_str());
				write_msg(cgisock,buf);
				host[i].status = F_COMMAND;
				WSAAsyncSelect(host[i].sockfd, hwnd, WM_SOCKET_NOTIFY,FD_WRITE );
				break;
			}
		}
		char buf[BufSize];
		memset(buf,0,BufSize);
		read_status = recv(host[i].sockfd, buf, BufSize,0);
		if(read_status > 0)
			host[i].sockStr.append(buf);
		else if(read_status == -1 && WSAGetLastError() == WSAEWOULDBLOCK)
			break;
		else if(read_status == -1 && WSAGetLastError() != WSAEWOULDBLOCK){

			closesocket(host[i].sockfd);
			WSACleanup();
			exit(1);
		}
		else if (read_status == 0){

			host[i].is_on = false;
			host[i].is_done = true;
			connection_num--;
			closesocket(host[i].sockfd);
			EditPrintf(hwndEdit, TEXT("host[%d] closed\r\n"),i);
			if(connection_num == 0){

				EditPrintf(hwndEdit, TEXT("DONE\r\n"),i);
				write_msg(cgisock,"</font>");
				write_msg(cgisock,"</body>");
				write_msg(cgisock,"</html>");
				closesocket(cgisock);
			}
			break;
		}
	}
}

void read_batch(int i, HWND hwnd, HWND hwndEdit)
{
	// Read from batch
	string line;
	getline(host[i].batch_stream,line);
	if(line.size() == 0){

		EditPrintf(hwndEdit, TEXT("read_batchError\r\n"),i);
		return;
	}
	if(line[line.size()-1]=='\r')
		line = line.substr(0,line.size()-1);	// remove /r;

	char buf[BufSize];
	memset(buf,0,BufSize);
	sprintf(buf,"<script>document.all['m%d'].innerHTML += \"<b><xmp>%s</xmp></b><br>\";</script>\n",i,line.c_str());
	write_msg(cgisock,buf);
	line = line + '\n';
	write_msg(host[i].sockfd,line.c_str());

	if(line == "exit\n"){

		host[i].is_done = true;
		host[i].batch_stream.close();

		while(true){

			char exitMsg[BufSize];		
			memset(exitMsg,0,BufSize);

			int read_status = recv(host[i].sockfd, exitMsg, BufSize,0);

			if((read_status==-1) && (WSAGetLastError() == WSAEWOULDBLOCK)){

				EditPrintf(hwndEdit, TEXT("block\r\n"),i);
				continue;
			}
			else if(read_status > 0)
			{
				unsigned r;
				host[i].sockStr.assign(exitMsg);
				while((r = host[i].sockStr.find('\r')) != -1)
					host[i].sockStr.erase(r,1);
				while(host[i].sockStr.size()!=0){

					r = host[i].sockStr.find('\n');
					char exitScript[BufSize];
					memset(exitScript,0,BufSize);
					if(r != -1){

						string line = host[i].sockStr.substr(0,r);// no \n
						sprintf(exitScript,"<script>document.all['m%d'].innerHTML += \"<xmp>%s</xmp><br>\";</script>\n",i,line.c_str());
						write_msg(cgisock,exitScript);
						host[i].sockStr.erase(0,r+1);
					}
					else{

						sprintf(exitScript,"<script>document.all['m%d'].innerHTML += \"<xmp>%s</xmp><br>\";</script>\n",i,host[i].sockStr.c_str());
						write_msg(cgisock,exitScript);
						host[i].sockStr.assign("");
					}
				}
			}
			else
			{
				host[i].is_on = false;
				host[i].is_done = true;
				connection_num--;
				closesocket(host[i].sockfd);
				EditPrintf(hwndEdit, TEXT("host[%d] closed(Project1)\r\n"),i);
				if (connection_num == 0){

					EditPrintf(hwndEdit, TEXT("DONE\r\n"),i);
					write_msg(cgisock,"</font>");
					write_msg(cgisock,"</body>");
					write_msg(cgisock,"</html>");
					closesocket(cgisock);
				}
				break;
			}
		}
	}

	WSAAsyncSelect(host[i].sockfd, hwnd, WM_SOCKET_NOTIFY,FD_READ);
}
