/*
 * client.cpp
 *
 *  Created on: 13-Nov-2015
 *      Author: altairmn
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include "dnsutils.h"
#include "base64.h"
#include <vector>
#include <sstream>
#include <string>
#include <fstream>
#include <streambuf>
#include <errno.h>
#include <signal.h>

using namespace std;


struct sigaction action;					/* for timeout */


#define BUFSIZE 65536
#define URLSIZE 1024
#define MXPAGE 353
#define MAXSEG 63
#define CHECKSEG 40
#define URLNUM	40

static const char SERV_ADDR[] = "192.168.35.52";
//static const char SERV_ADDR[] = "127.0.0.1";
const string TUNNEL = ".t1.sahilmn.com";

enum state {
	recving,
	sending,
	s_reset
};

enum status {
	start,
	cont,
	end,
	invalid,
	st_reset
};


int getTXTrequest(char *url, char *req, int seq) {


	char *queryname;
	struct dns_header *dns;
	struct question *info;


	dns = (struct dns_header*) req;
	setdnsheader(dns);												// Fill buffer with DNS Header
	dns->iden = (unsigned short) htons(seq);

	queryname = (char *) (req + sizeof(struct dns_header));			// Add queryname to  buffer
	dnsname(queryname, url);										// Get host in DNS format

	int tot = sizeof(struct dns_header) + strlen(queryname) + 1;	// 1 zero byte after queryname

	int type_txt = 16;
	info = (struct question*) (req + tot);						// Add question type and class to buffer
	info->qtype = htons(type_txt);
	info->qclass = htons(1);
	tot += sizeof(struct question);

	return tot;
}

void timerinterrupt(int sig) {
	fprintf(stderr, "timeout\n");
}

void clientInit(int *sock, struct sockaddr_in* serv) {

	/* creating socket */
	if((*sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("Socket creation failed");
			exit(1);
	}

	/* getting server for sending */
	serv->sin_family = AF_INET;
	serv->sin_port = htons(53);						// Set port no. 53 which is reserved for DNS
	serv->sin_addr.s_addr = inet_addr(SERV_ADDR);

	/* setting up action*/
	sigemptyset(&action.sa_mask);
	action.sa_handler = timerinterrupt;

	if (sigaction(SIGALRM, &action, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

}

int getDATA(char **data) {
	char path[100];
	scanf("%s", path);

	size_t len;	/* length of URL */
	std::ifstream t(path);
	std::stringstream buffer;
	buffer << t.rdbuf();

	string temp = buffer.str();
	*data = (char *) malloc(temp.length());
	strcpy(*data, temp.c_str());

	return temp.length();
}



int sendRequest(int sock, sockaddr_in serveraddr, char *data, int data_len) {

	char req[BUFSIZE];
	char res[BUFSIZE];

	//char *enc_data;
	//int enc_len;

	//enc_data = base64(data, data_len, &enc_len);
	
	//string enc_str(enc_data);

	vector <string> split_data;
	//for(int i = 0; i < enc_len; i += MAXSEG - 2) {
	//	string temp = "c" + enc_str.substr(i, MAXSEG - 2) + "c" ;
	//	split_data.push_back(temp);
	//}
	//
	/* experimental code */
	string data_str(data);
	for(int i = 0; i < data_len; i += CHECKSEG) {
		string tmp = data_str.substr(i, CHECKSEG);
		int plen;
		char *part = base64(tmp.c_str(), tmp.length(), &plen);
		string str_temp = "c";
		str_temp.append(part);
		str_temp.append("c");
		split_data.push_back(str_temp);
		free(part);
	}

	int sz = split_data.size();
	int len = split_data[sz-1].length();
	split_data[0][0] = 's';
	split_data[sz-1][len-1] = 'e';

	char data_char[URLSIZE];



	int sb, rb;
	socklen_t servlen = sizeof(serveraddr);

	for(int i = 0; i < sz; ++i) {

		split_data[i].append(TUNNEL);

		strcpy(data_char, split_data[i].c_str());
		

		int req_size = getTXTrequest(data_char, req, i);

		fprintf(stderr, "sending request on socket %d\n", sock);

		/* send response */
	label:
		alarm(5);
		if((sb = sendto(sock, req, req_size, 0, (struct sockaddr*)&serveraddr, servlen)) < 0) {
			perror("Send failed");
			exit(1);
		}

		/* wait for response */
		if((rb = recvfrom(sock, res, BUFSIZE, 0, (struct sockaddr*)&serveraddr, &servlen)) < 0) {
			if(errno == EINTR) {
				goto label;
			}
			perror("Error in receive\n");
			exit(1);
		}


		/* ignore dummy responses */
	}

	alarm(0);

	return 0;

}


int main(int argc, char *argv[]) {

	int udpsock;
	struct sockaddr_in serveraddr;


	/* initialize client */
	clientInit(&udpsock, &serveraddr);

	/* get input from user */
	char *data;
	int data_len;
	data_len = getDATA(&data);

	/* send request for URL */
	sendRequest(udpsock, serveraddr, data, data_len);


	fprintf(stderr, "response received\n");


	return 0;
}
