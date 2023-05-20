/***************************************************************************
 *  Small CoT simulator, based on cURL example.     
 * 
 *  Copyright (C) 2022, Resilience Theatre
 * 
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2021, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/* <DESC>
 * An example of curl_easy_send() and curl_easy_recv() usage.
 * https://curl.se/libcurl/c/sendrecv.html
 * </DESC>
 */

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h> 
#include <stdlib.h>
#include <time.h>
#include "log.h"
#include "ini.h"

char *server_address = "";
char *client_cert="";
char *client_key="";
char *client_key_password="";
char *ca_cert="";
char *simulation_file="";
char *simulation_target="";
char *interval_wait="";

/* Auxiliary function that waits on the socket. */
static int wait_on_socket(curl_socket_t sockfd, int for_recv, long timeout_ms)
{
  struct timeval tv;
  fd_set infd, outfd, errfd;
  int res;
  
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  FD_ZERO(&infd);
  FD_ZERO(&outfd);
  FD_ZERO(&errfd);

  FD_SET(sockfd, &errfd); /* always check for error */

  if(for_recv) {
    FD_SET(sockfd, &infd);
  }
  else {
    FD_SET(sockfd, &outfd);
  }

  /* select() returns the number of signalled sockets or -1 */
  res = select((int)sockfd + 1, &infd, &outfd, &errfd, &tv);
  return res;
}

int iso8601(char* string) {
    time_t rawtime = time(NULL);

    if (rawtime == -1) {
        return 0;
    }
    
    struct tm *ptm = localtime(&rawtime);

    if (ptm == NULL) {
        return 0;
    }
    
    sprintf(string, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", (ptm->tm_year + 1900), (ptm->tm_mon + 1), ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    return 1;
}

int send_cot_message(char *lat, char *lon, char* simulation_target) {
		
	log_debug("[%d] send_cot_message() #0 ",getpid());
	
	CURL *curl;
	char request[2000];
	
	log_debug("[%d] send_cot_message() #1 ",getpid());
	
	/* CoT template */
	sprintf(request,"<event version='2.0' uid='' type='a-f-G-U-C' how='' \
	time='2022-02-20T05:33:35.501Z' start='2022-02-20T05:33:35.501Z' \
	stale='2022-02-20T05:39:50.501Z'> <point lat='%s' lon='%s' hae='31.4' \
	ce='176.9' le='9999999.0'/><detail> \
	<takv os='30' version='4.5.1.4 (84421720)[playstore].1643069570-CIV' device='HMD GLOBAL NOKIA 6.2' platform='ATAK-CIV'/> \
	<contact endpoint='*:-1:stcp' phone='0414805697' callsign='%s'/><uid Droid='GOLAN'/><precisionlocation altsrc='GPS' geopointsrc='GPS'/> \
	<group role='Team Member' name='Cyan'/><status battery='98'/><track course='175.40962171230342' speed='0.0'/> \
	</detail> \
	</event>",lat,lon,simulation_target);

	size_t request_len = strlen(request);
	log_debug("[%d] send_cot_message() #2 ",getpid());
	curl = curl_easy_init();
	log_debug("[%d] send_cot_message() #3 ",getpid());
	
	
	if(curl) {
		CURLcode res;
		curl_socket_t sockfd;
		size_t nsent_total = 0;
		struct curl_slist *dns;
		dns = curl_slist_append(NULL, "buildroot:8089:127.0.0.1");
		curl_easy_setopt(curl, CURLOPT_RESOLVE, dns);	
		curl_easy_setopt(curl, CURLOPT_URL, server_address);
		/* TLS */ 
		curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
		curl_easy_setopt(curl, CURLOPT_SSLCERT, client_cert);
		if(client_key_password) {
			curl_easy_setopt(curl, CURLOPT_KEYPASSWD, client_key_password);
		}
		curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
		curl_easy_setopt(curl, CURLOPT_SSLKEY, client_key);
		curl_easy_setopt(curl, CURLOPT_CAINFO, ca_cert);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTP09_ALLOWED, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
		  log_error("[%d] curl error: %s",getpid(),curl_easy_strerror(res) ); 
		  return 1;
		}
		/* Extract the socket from the curl handle - we will need it for waiting. */
		res = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);
		if(res != CURLE_OK) {
			log_error("[%d] curl error: %s",getpid(),curl_easy_strerror(res) ); 
			return 1;
		}
		log_info("[%d] Sending CoT message",getpid() ); 
		
		do {
		  /* Warning: This example program may loop indefinitely.
		   * A production-quality program must define a timeout and exit this loop
		   * as soon as the timeout has expired. */
		  size_t nsent;
		  do {
			nsent = 0;
			res = curl_easy_send(curl, request + nsent_total,request_len - nsent_total, &nsent);
			nsent_total += nsent;
			if(res == CURLE_AGAIN && !wait_on_socket(sockfd, 0, 60000L)) {
			  log_error("[%d] Timeout",getpid() ); 
			  return 1;
			}
		  } while(res == CURLE_AGAIN);
		  if(res != CURLE_OK) {
			log_error("[%d] curl error: %s",getpid(),curl_easy_strerror(res) ); 
			return 1;
		  }
		  log_info("[%d] Sent %lu bytes.",getpid(),(unsigned long)nsent );

		} while(nsent_total < request_len);
}
	// end of send
    /* always cleanup */
    curl_easy_cleanup(curl);
    return 0;
	
}

int main(int argc, char *argv[])
{
	char *ini_file=NULL;
	int c=0;
	int log_level=LOG_INFO;
	
	while ((c = getopt (argc, argv, "dhi:")) != -1)
	switch (c)
	{
		case 'd':
			log_level=LOG_DEBUG;
			break;
		case 'i':
			ini_file = optarg;
			break;
		case 'h':
			log_info("[%d] cotsim - Cursor on Target simulator ",getpid());
			log_info("[%d] Usage: -i [ini_file] ",getpid());
			log_info("[%d]        -d debug log ",getpid());
			log_info("[%d] ",getpid());
			log_info("[%d] Simulation file is csv file with lat,lon on each row",getpid());
			log_info("[%d] You can use 'gpsbabel' to convert files to csv format:",getpid());
			log_info("[%d] gpsbabel -i gpx -f [input].gpx -o csv -F [output].csv",getpid()); 
			return 1;
		break;
			default:
			break;
	}
	if (ini_file == NULL) 
	{
		log_error("[%d] ini file not specified, exiting. ", getpid());
		return 0;
	}
	/* Set log level: LOG_INFO, LOG_DEBUG */
	log_set_level(log_level);
	/* read ini-file */
	ini_t *config = ini_load(ini_file);
	ini_sget(config, "cotsim", "server_address", NULL, &server_address);
	ini_sget(config, "cotsim", "client_cert", NULL, &client_cert);
	ini_sget(config, "cotsim", "client_key", NULL, &client_key);
	ini_sget(config, "cotsim", "client_key_password", NULL, &client_key_password);
	ini_sget(config, "cotsim", "ca_cert", NULL, &ca_cert);
	ini_sget(config, "cotsim", "simulation_file", NULL, &simulation_file); 
	ini_sget(config, "cotsim", "simulation_target", NULL, &simulation_target); 	
	ini_sget(config, "cotsim", "interval_wait", NULL, &interval_wait); 	
	
	log_info("[%d] Server address: %s ",getpid(),server_address);
	log_info("[%d] Client cert %s",getpid(),client_cert);
	log_info("[%d] Client key: %s",getpid(),client_key);
	log_info("[%d] CA-cert: %s ",getpid(),ca_cert);
	log_info("[%d] Simulation file: %s ",getpid(),simulation_file); 
	log_info("[%d] Simulation target: %s ",getpid(),simulation_target); 
	log_info("[%d] Interval (s): %s ",getpid(),interval_wait); 
	
	log_debug("[%d] Client key password: %s ",getpid(),client_key_password);
	
	// 
	// Read file 
	// 
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen(simulation_file, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
	}

	log_debug("[%d] fopen_done",getpid());
    while ((read = getline(&line, &len, fp)) != -1) {
		log_debug("[%d] while #1 ",getpid());
		// Parse LAT, LON
		char *pt;
		pt = strtok (line,",");
		int x = 0;
		char lat[100];
		char lon[100];
		while (pt != NULL) {			
			if (x == 0) {
				strcpy(lat,pt);
			}
			if (x == 1) {
				strcpy(lon,pt);
			}
			x++;
			pt = strtok (NULL, ",");
		}
		log_debug("[%d] while #2 %s %s %s",getpid(),lat,lon,simulation_target);
		send_cot_message(lat,lon,simulation_target);
		log_debug("[%d] sent cot ",getpid());
		x=0;
		sleep(atoi(interval_wait));
    }
    fclose(fp);
    if (line)
        free(line);
    exit(EXIT_SUCCESS);
  return 0;
}
