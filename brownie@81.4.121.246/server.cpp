/*
 * server.cpp
 *
 *  Created on: 13-Nov-2015
 *      Author: altairmn
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>			/* for logging */
#include "dnsutils.h"		/* misc. dns related functionality */
#include "base64.h"			/* for encoding/decoding */
#include "pageget.h"		/* for downloading the requested page */
#include <errno.h>
#include <signal.h>

#define BUFSIZE 65536
#define MAX_DNS_PKT_SIZE 576
#define URL_SIZE 1024
#define MAXSEG 63
#define TXT_SEG_LEN 200



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

/* global variables */
static const char * __log_ident = "[SERVER]";
static enum state mode;

using namespace std;



/* return length of URL. */
int getencURLPart(unsigned char *ureq, unsigned char *uenc_urlp, enum status* flag) {
	char *req = (char *) ureq;
	char *enc_urlp = (char *) uenc_urlp;
	char url[URL_SIZE];
	char wrapped_urlp[MAXSEG + 1];

	if(flag == NULL) {
		fprintf(stderr, "NULL POINTER");
		exit(1);
	}
	enum status stat = *flag;

	/* skipping header */
	int offset = sizeof(struct dns_header);

	/* copy url from req to url[] */
	strcpy(url, req + offset);

	int wr_len = url[0];
	strncpy(wrapped_urlp, req + offset + 1, wr_len);

	wrapped_urlp[wr_len] = '\0';
	fprintf(stderr, "wr_encode_url : %s\n", wrapped_urlp);

	if(wrapped_urlp[wr_len - 1] == 'c')
		stat = cont;
	else if(wrapped_urlp[wr_len - 1] == 'e')
		stat = end;
	else
		stat = invalid;


	string temp(wrapped_urlp);

	strcpy(enc_urlp, temp.substr(1, temp.length() - 2).c_str());
	fprintf(stderr, "encode_url : %s\n", enc_urlp);

	*flag = stat;
	return strlen(enc_urlp);

}

int getTXTresponse(unsigned char *ureq, unsigned char *ures, char *txt) {

	char *req = (char *) ureq;
	char *res = (char *) ures;

	/* Handling the header */
	struct dns_header *req_dns, *res_dns;
	memcpy(res, req, sizeof(struct dns_header));
	res_dns = (struct dns_header *) res;
	res_dns->ans_count = htons(1);
	res_dns->add_count = htons(0);
	res_dns->authans = 1;
	res_dns->queryresp = 1;

	int offset = sizeof(struct dns_header);	/* calculating offset for future use */
	unsigned short name_pos = 49152 | (offset);	/* position of query name for dns (1100 0000 0000 0000 | position)*/

	/* copying the query name string */
	strcpy(res + offset, req + offset);

	offset += strlen(req + offset) + 1;	/* recomputing offset with query string added */
	memcpy(res + offset, req + offset, sizeof(struct question));	/* copying type and class */

	struct question *info = (struct question *) (req + offset);
	offset += sizeof(struct question);

	/* begin answer in response */
	struct answer_rr *answer = (struct answer_rr *) (res + offset);
	answer->ptr = htons(name_pos);
	answer->atype = info->qtype;
	answer->aclass = info->qclass;
	answer->ttl_msb = htons(0);
	answer->ttl_lsb = htons(0);
	answer->len = htons(1 + strlen(txt));

	offset += sizeof(answer_rr);

	*(res + offset) = strlen(txt);
	offset++;

	/* putting text into the response */
	memcpy(res + offset, txt, strlen(txt));
	offset += strlen(txt);

	return offset;
}

/* serverInit :
 * @param sock => socket fd. get and store.
 * @param serveraddr => set serveraddr pointer.
 *
 * The method binds the serveraddr to socket fd. If the server is bound succesfully,
 * function returns.
 */

void timerinterrupt(int sig) {
	syslog(LOG_INFO, "timeout");
}

void serverInit(int *sock, struct sockaddr_in* serveraddr) {

	if((*sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("Socket creation failed");
		exit(1);
	}

	/* Fill serveraddr with port number and address */
	serveraddr->sin_family = AF_INET;
	serveraddr->sin_port = htons(53);
	serveraddr->sin_addr.s_addr=INADDR_ANY;	/* listen on all interfaces */
	memset(serveraddr->sin_zero, '\0', sizeof serveraddr->sin_zero);

	/* Bind socket */
	if(bind(*sock, (struct sockaddr*) serveraddr, sizeof(struct sockaddr_in)) < 0) {
		perror("Bind:");
		exit(1);
	}

	/* initializing logging */
	openlog(__log_ident, LOG_PERROR, LOG_USER);

	fprintf(stderr, "Server running on port 53\n");

	return;

}

int recvRequest(int sock, unsigned char *data) {

	/* declaring and initializing to 0*/
	unsigned char req[MAX_DNS_PKT_SIZE];	/* request buffer */
	unsigned char res[MAX_DNS_PKT_SIZE];	/* response buffer */
	memset(req, 0, MAX_DNS_PKT_SIZE);
	memset(res, 0, MAX_DNS_PKT_SIZE);

	/* rb: received bytes. sb: sent bytes */
	int rb, sb;

	int url_len;

	/* for storing client information */
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);

	/* indicator enums */
	enum status stat = start;

	/* misc variables for temporary use */
	int url_part_len = 0;					// stores length of part of url
	unsigned char url_part[URL_SIZE];	// stores part of url
	string url_accumulator("");			// string for URL accumulation
	char filler_txt[] = "txt response";


	/* experimental code */
	int dec_url_part_len = 0;		// exp
	unsigned char *data_part;				// exp

	/* receiving URL in loop */
	while(mode == recving) {

		/* Receive request from client */
		if((rb = recvfrom(sock, req, BUFSIZE, 0, (struct sockaddr*)&clientaddr, &len)) < 0) {
			perror("Error in receive\n");
			exit(1);
		}

		syslog(LOG_INFO, "Request received from %s : %d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
		// fprintf(stderr, "Request received from %s : %d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);

		url_part_len = getencURLPart(req, url_part, &stat);		/* extract url part from request */
		
		string sh_temp_str("");		// exp
		sh_temp_str.append((char * )url_part);

		data_part = unbase64(sh_temp_str.c_str(), sh_temp_str.length(), &dec_url_part_len);		// exp
		
		string temp2("");				// exp
		temp2.append((char *) data_part);// exp
		fprintf(stderr, "decoded : %s\n", temp2.c_str());

		url_accumulator.append((char *)data_part);				/* append urlpart to accumulator */	// exp

		free(data_part);		// exp

		/* get filler response */
		int res_len = getTXTresponse(req, res, filler_txt);

		/* send response to client*/
		if((sb = sendto(sock, res, res_len, 0, (struct sockaddr*)&clientaddr, len)) < 0) {
			perror("Error in send:");
			exit(1);
		}

		syslog(LOG_INFO, "Response sent to client");
		//fprintf(stderr, "sent response\n");

		/* Checking if more packets expected */
		if(stat == end) {
			mode = sending;
			syslog(LOG_INFO, "Received complete URL");
		}
		else if(stat == invalid) {
			mode = s_reset;
			return 0;
		}
	}


	//data = unbase64(url_accumulator.c_str(), url_accumulator.length(), &url_len);
	syslog(LOG_INFO, "Received URL : %s", url_accumulator.c_str());


	return 0;
}




int main(int argc,char **argv) {

	int udpsock;					/* socket server listens on */
	struct sockaddr_in serveraddr;	/* addr info for server */
	unsigned char *data;				/* for storing request URLS. url size is max 1024 chars */

	/* Initializing the server
	 *  - get socket
	 *  - fill in serveraddr (port : 53, interface : ANY_ADDR
	 */
	serverInit(&udpsock, &serveraddr);

	/* the server runs within a loop */
	//while(1) {

	mode = recving;
	/* listen for requests */
	syslog(LOG_INFO, "Listening for requests");
	recvRequest(udpsock, data);


	free(data);


	return 0;
}

