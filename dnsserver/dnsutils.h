/*
 * dns_types.h
 *
 *  Created on: 13-Nov-2015
 *      Author: altairmn
 */

#ifndef DNSUTILS_H_
#define DNSUTILS_H_

/* Struct for DNS HEADER */
struct dns_header
{
    unsigned short iden; // identification number

    unsigned char recd :1;   		// recursion desired
    unsigned char trunc :1;  		// truncated message
    unsigned char authans :1;  		// authoritative answer
    unsigned char opcode :4;  		// purpose of message
    unsigned char queryresp :1; 	// query/response flag

    unsigned char respcode :4; 		// response code
    unsigned char checkdsbld :1; 	// checking disabled
    unsigned char authdata :1; 		// authenticated data
    unsigned char zero :1; 			// its zero
    unsigned char recavail :1; 		// recursion available

    unsigned short q_count; 		// no. of questions
    unsigned short ans_count; 		// no. of non-authoritative answers
    unsigned short auth_count; 		// no. of authoritative answers
    unsigned short add_count; 		// no. of additional answers
};

/* struct for DNS question */
struct question{
	unsigned short qtype, qclass;
};

/* struct for received answer */
struct answer_rr{
	unsigned short ptr;
	unsigned short atype;
	unsigned short aclass;
	unsigned short ttl_msb;
	unsigned short ttl_lsb;
	unsigned short len;
};

/*Function to change query name to dns format 3www9gradiance3com **/

void dnsname(char* sret, char* shost)
{
	unsigned char* ret = (unsigned char *) sret;
	unsigned char* host = (unsigned char *) shost;

    int last = 0 , i;
    strcat((char*)host,".");

    for(i = 0 ; i < strlen((char*)host) ; i++)
    {
        if(host[i]=='.')		// read string till "." is found
        {
            *ret++ = i-last;
            for(;last<i;last++)
            {
                *ret++=host[last];
            }
            last=i+1;
        }
    }
    *ret++='\0';
}

/* Function to set values in dnsheader */
void setdnsheader(struct dns_header* curdns){

    curdns->iden = (unsigned short) htons(getpid());		// Identification set to process id
    curdns->queryresp = 0;
    curdns->opcode = 0;
    curdns->authans = 0;
    curdns->trunc = 0;
    curdns->recd = 1;
    curdns->recavail = 0;
    curdns->zero = 0;
    curdns->authdata = 0;
    curdns->checkdsbld = 0;
    curdns->respcode = 0;
    curdns->q_count = htons(1);								//  Only 1 question
    curdns->ans_count = 0;
    curdns->auth_count = 0;
    curdns->add_count = 0;
}


#endif /* DNSUTILS_H_ */
