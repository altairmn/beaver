/*
 * pageget.h
 *
 *  Created on: 15-Nov-2015
 *      Author: altairmn
 */

#ifndef PAGEGET_H_
#define PAGEGET_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <fstream>
#include <streambuf>


using namespace std;

#define PG_CMD_SIZE 1024
#define URLSIZE 1024

size_t trimwhitespace(char *out, size_t len, const char *str) {
  if(len == 0)
    return 0;

  const char *end;
  size_t out_size;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
  {
    *out = 0;
    return 1;
  }

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  end++;

  // Set output size to minimum of trimmed string length and buffer size minus 1
  out_size = (end - str) < len-1 ? (end - str) : len-1;

  // Copy trimmed string and add null terminator
  memcpy(out, str, out_size);
  out[out_size] = 0;

  return out_size;
}

long getPage(char *url, char **page) {

	/* download page using cURL */
	char tr_url[URLSIZE];
	trimwhitespace(tr_url, strlen(url), url);
	char command[PG_CMD_SIZE] = "curl ";
	strcat(command, tr_url);
	strcat(command, " > testpage.txt");
	system(command);

	std::ifstream t("testpage.txt");
	std::stringstream buffer;
	buffer << t.rdbuf();

	string temp = buffer.str();
	*page = (char *) malloc(temp.length());
	strcpy(*page, temp.c_str());

	// fprintf(stderr, "page having length %ld is %s\n", temp.length(), *page);

	return temp.length();
}


#endif /* PAGEGET_H_ */
