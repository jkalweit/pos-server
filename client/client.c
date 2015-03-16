#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>



int portno, done, sock_publish, sock_subscribe;
struct sockaddr_in serv_addr_subscribe, serv_addr_publish;
struct hostent *server;

char GUID[37];



void error(const char *msg)
{
    perror(msg);
    exit(0);
}


// GUID is 37 chars including null terminator
void create_guid(char *GUID) {
    srand (clock());
    int t = 0;
    char *szTemp = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    char *szHex = "0123456789ABCDEF-";
    int nLen = strlen (szTemp);

    for (t=0; t<nLen+1; t++)
    {
        int r = rand () % 16;
        char c = ' ';

        switch (szTemp[t])
        {
            case 'x' : { c = szHex [r]; } break;
            case 'y' : { c = szHex [r & 0x03 | 0x08]; } break;
            case '-' : { c = '-'; } break;
            case '4' : { c = '4'; } break;
        }

        GUID[t] = ( t < nLen ) ? c : 0x00;
    }
}


void getMessage(char buffer[])
{
    printf("Please enter the message: ");
    fgets(buffer,255,stdin);
    int length = strlen(buffer);
    buffer[length-1] = '\0'; //replace newline with null terminator
}

void sendMessage(char buffer[], int sockfd)
{
    int length = strlen(buffer);
    int n = write(sockfd,buffer,length);
    if (n < 0)
        error("ERROR writing to socket");
}

void receiveMessage(char buffer[], int sockfd)
{
    int n = read(sockfd,buffer,255);
    buffer[n] = '\0';
    if (n < 0)
        error("ERROR reading from socket");
}


void *receive_updates(void *arg) {

    sock_subscribe = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_subscribe < 0)
        error("ERROR opening subscribe socket");

    if (connect(sock_subscribe,(struct sockaddr *) &serv_addr_subscribe,sizeof(serv_addr_subscribe)) < 0)
        error("ERROR subscribe connecting");


    char buffer[256];
    while(!done)
    {
        receiveMessage(buffer, sock_subscribe);
        if(strlen(buffer) == 0) {
            printf("Subscribe socket closed.\n");
            done = 1;
        } else {
            printf("Received Update: %s\n", buffer);
        }
    }

    close(sock_subscribe);

    return;
}



void send_updates() {

    sock_publish = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_publish < 0)
        error("ERROR opening publish socket");

    if (connect(sock_publish,(struct sockaddr *) &serv_addr_publish,sizeof(serv_addr_publish)) < 0)
        error("ERROR connecting");

    char message[256];
    char buffer[256];
    while(!done)
    {
        getMessage(message);
        if(message[0] == 'q')
        {
            done = 1;
        }
        else
        {
            sprintf(buffer, "%s %s", GUID, message);
            sendMessage(buffer, sock_publish);
            receiveMessage(buffer, sock_publish);
            int length = strlen(buffer);
            if(length == 0) {
                printf("Publish socket closed.\n");
                done = 1;
            } else {
                printf("Publish response from server: %d: %s\n", length, buffer);
            }
        }
    }

    close(sock_publish);
}








int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = atoi(argv[2]);


    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    memset((char *) &serv_addr_subscribe, 0, sizeof(serv_addr_subscribe));
    serv_addr_subscribe.sin_family = AF_INET;
    memcpy((char *)&serv_addr_subscribe.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    memcpy((char *) &serv_addr_publish, (char *) &serv_addr_subscribe, sizeof(serv_addr_subscribe));


    serv_addr_subscribe.sin_port = htons(portno);
    serv_addr_publish.sin_port = htons(portno + 1);


    create_guid(GUID);
    printf("GUID: %s", GUID);

    // receive loop
    pthread_t rcv_thread;
    pthread_create(&rcv_thread, NULL, receive_updates, NULL);


    // main event loop
    send_updates();


    return 0;
}
