// Websnarf
// PERRIGOT Matthieu
// GOETZ Alexandre
// VASAUNE Christian

#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

char* get_month_char(int month){
    char *months[12] = { 
	  "Jan",
	  "Feb",
	  "Mar",
	  "Apr",
	  "May",
	  "Jun",
	  "Jul",
	  "Aug",
	  "Sep",
	  "Oct",
	  "Nov",
	  "Dec"
	};
    return months[month];
}


int main (int argc, char *argv[]) {
	int debug = 0;
    int apache = 0;
    int iis = 0;
    int ncsa = 0;
    int nooutput = 0;
	u_short port = 80;
	int alarmtime = 5;
	int maxline = 40;
	char version[] = "1.04";
	char logfile[50] = {0};
	char savedir[100] = {0};

    // Récupération de la date et de l'heure
	time_t t = time(NULL);
    struct tm current_time = *localtime(&t);
	//fflush(stdout);

	// OPTIONS
	/*
	usage: $0 [options]

	--timeout=<n>   wait at most <n> seconds on a read (default $alarmtime)
	--log=FILE      append output to FILE (default stdout only)
	--port=<n>      listen on TCP port <n> (default $port/tcp)
	--max=<n>       save at most <n> chars of request (default $maxline chars)
	--save=DIR      save all incoming headers into DIR
	--debug         turn on a bit of debugging (mainly for developers)
	--apache        logs are in Apache style
	--iis 			logs are in IIS style
	--ncsa			logs are in NCSA style
	--version       show version info
	--nooutput		nothing is printed in the terminal
	--help          show this listing 
	*/
	
    int tmpi = 0;

	if (argc > 1){
		for(int i = 1; i < argc; i++){
            //--help
			if (strstr(argv[i],"--help")){
				printf("usage: $0 [options]\n\n");
				exit(0);
			}
            //--log=FILE
			else if (strstr(argv[i],"--log=")){
                // 6 =  position du "=", ici 6
				 memcpy(logfile,&argv[i][6],strlen(argv[i]) - 6);
			}
            
            //--port=<n>
			else if (strstr(argv[i], "--port=")){
                char tmp[6];
                memcpy(tmp,&argv[i][7],strlen(argv[i]) - 7);
				port = atoi(tmp);
            }
            //--timeout=<n>
            else if (strstr(argv[i],"--timeout=")){ 
                char tmp[6];
                memcpy(tmp,&argv[i][10],strlen(argv[i]) - 8);
				alarmtime = atoi(tmp);
			}
			//--max=<n>
			else if (strstr(argv[i] ,"--max=")){
				char tmp[6];
                memcpy(tmp,&argv[i][6],strlen(argv[i]) - 6);
			}
			//--save=DIR
			else if (strstr(argv[i],"--save=")){
        		memcpy(savedir,&argv[i][7],strlen(argv[i]) - 7);
			}
			//--debug
			else if (strstr(argv[i],"--debug")){
				debug = 1;
			}
			//--apache
			else if (strstr(argv[i],"--apache")){
				apache = 1;
			}
			//--iis
			else if (strstr(argv[i],"--iis")){
				iis = 1;
			}
			//--ncsa
			else if (strstr(argv[i],"--ncsa")){
				ncsa = 1;
			}
			//--nooutput
			else if (strstr(argv[i],"--nooutput")){
				nooutput = 1;
			}
			//--version
			else if (strstr(argv[i],"--version")){
				printf("websnarf v$version -- http://www.unixwiz.net/tools websnarf.html\n");
				exit(0);
			}
			else {
				printf("	usage: $0 [options]\n");
			}
		}
	}
    // ##### Fichier de log #####
    FILE *file;
    char str_port[6];
    sprintf(str_port,"%d",port);
    // Create Logfile
    if(strlen(logfile) != 0){
        // Création du fichier de log en mode écriture
        if(strlen(savedir) !=0){
            int verif = mkdir(savedir,0700);
            strcat(savedir,logfile);
            strcpy(logfile,savedir);
        }
        
        file = fopen(logfile,"a");
        
        if(file == NULL){
            perror("ERROR: cannot create logfile\n");
            return -1;
        }
        // int to String
        // écriture de la première ligne
        fputs("# Now listening on port ",file);
        fputs(str_port,file);
        fputs("\n",file);
        fclose(file);
    }

/* Create Socket */
	struct sockaddr_in adresse, client_adresse;
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*) & client_adresse;
    struct in_addr ipAddr = pV4Addr->sin_addr;
    struct hostent *recup;
	int sock, len;
	int newsockfd;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("ERROR : cannot create socket\n");
        return(-1);
    }
    
    int optval;
    optval = 1; 
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval)<0){
        perror ("ERROR: cannot apply option for the socket\n");
        return(-1); 
    }

    adresse.sin_family = AF_INET;
    adresse.sin_port = htons(port);  
    adresse.sin_addr.s_addr=INADDR_ANY;
  	
  	int ret = -1;
    ret = bind (sock,(struct sockaddr *)&adresse,sizeof(adresse));
    if( ret < 0){
        perror("ERROR: cannot name socket\n");
        return(-1);
    }
    
    if(nooutput!=1){
		printf("websnarf v%s listening on port %d (timeout=%d secs)\n", version, port, alarmtime);
  	}

  	listen(sock, 5);
	
  	int timeout = 0;
    char peerhost[INET_ADDRSTRLEN];
  	char sockhost[INET_ADDRSTRLEN]; // our own IP address
  	char *request;
    request = malloc(1000);
    char *stline_request;
    stline_request = malloc(1000);
  	int nreads = 0;
  	int tmout = alarmtime;
  	fd_set active_fd_set, read_fd_set;
  	FD_ZERO (&active_fd_set);
  	FD_SET (sock, &active_fd_set);
    
    struct   timeval tv;
    tv.tv_sec = alarmtime;
    tv.tv_usec = 0;
  	
    //#########################################
    //######### DEBUT DE L'ECOUTE #############
	//#########################################
    for(;;){
		// on accepte absolument tout en boucle
	    len = sizeof(&client_adresse);
        newsockfd = accept(sock, (struct sockaddr *) &client_adresse, &len);

        
	    if (newsockfd == -1) {
	      perror("ERROR : cannot create socket [2]\n");
	      return(-1);
	    }
        inet_ntop( AF_INET, &client_adresse.sin_addr, peerhost, INET_ADDRSTRLEN );
	    inet_ntop( AF_INET, &adresse.sin_addr, sockhost, INET_ADDRSTRLEN );
	    
	    if (debug == 1){
	    	printf("--> accepted connection from %s\n", peerhost);
		}
/* Go Read */
		read_fd_set = active_fd_set;
        
        ret = recv(newsockfd,request,1000,0);
        
        if( ret == -1){
            perror("ERROR: cannot read the request from the client");
        }
        else if(ret == 0){
            perror("ERROR: Client disconnected ");
        }
        char *end_of_line = strchr(request,'\n');
        if(end_of_line != NULL)
            memcpy(stline_request,request,end_of_line-request); 
        else
            strcpy(stline_request,request);
       	if (select (sock,&read_fd_set,NULL,NULL,&tv) < 0){
           perror ("ERROR : select");
           return(-1);
         }
        //éventuelle écriture des logs 
        if(strlen(logfile) != 0){
            file = fopen(logfile,"a");
            
            char buff[4] = {0};
            char log[10000] = {0};
            
            current_time = *localtime(&t);
            //format apache
            if(apache != 0){
                sprintf(log,"%s - - [%s/%02d/%04d] \"%s\" 404 100\n\0",peerhost,get_month_char(current_time.tm_mon),current_time.tm_mday,current_time.tm_year + 1900, stline_request);
            }
            // format iis
            else if (iis != 0){
            	sprintf(log,"%02d/%02d,%02d:%02d:%02d,%s,%s,%s\n\0",
                        (current_time.tm_mon+1),current_time.tm_mday,current_time.tm_hour
                       ,current_time.tm_min,current_time.tm_sec,sockhost,peerhost,stline_request);
            }
            // format ncsa
            else if (ncsa != 0){
            	sprintf(log,"%s - [%02d/%02d:%02d:%02d:%02d] \"%s\"\n\0", peerhost, 
                        (current_time.tm_mon+1),current_time.tm_mday,current_time.tm_hour
                       ,current_time.tm_min,current_time.tm_sec,stline_request);
            }
            else{ // format normal
                sprintf(log,"%02d/%02d %02d:%02d:%02d %s -> %s : %s \n\0",
                        (current_time.tm_mon+1),current_time.tm_mday,current_time.tm_hour
                       ,current_time.tm_min,current_time.tm_sec,sockhost,peerhost,stline_request);

            }
            
            fputs(log,file);
            printf("%s\n",log);
            fclose(file);
        }
	    close(newsockfd);
	}
    
    close(sock);
    
    // Fermeture du fichier de log 
    if(strlen(logfile) != 0){
        fputs("\n",file);
        fclose(file);
    }
    
    return(0);
}
