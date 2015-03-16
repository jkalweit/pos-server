#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"



#define PORT    3000
#define MAXMSG  1024


struct subscriber {
    char GUID[37];
};

struct subscriber subscribers[256];

int sock_incoming_subscribers, sock_incoming_publishers;


fd_set subscriber_fd_set, publisher_fd_set, listening_fd_set;


int
make_socket (uint16_t port)
{
    struct sockaddr_in name;

    /* Create the socket. */
    int sockid = socket (PF_INET, SOCK_STREAM, 0);
    if (sockid < 0)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (sockid, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
        perror ("bind");
        exit (EXIT_FAILURE);
    }
    return sockid;
}





FILE* open_file(char filename[])
{
    FILE *f = fopen(filename, "a");
    if (f == NULL)
    {
        printf("Error opening file: %s\n", filename);
        exit(1);
    }
    return f;
}





void do_command(char buffer[], int sockfd)
{
    FILE *f;
    char GUID[37];
    char command = '\0';
    char arg1[50];
    char payload[256];
    memset(arg1, 0, sizeof(arg1));
    memset(payload, 0, sizeof(payload));
    sscanf(buffer, "%s %c %49s %255[^|]", GUID, &command, arg1, payload);
    printf("Doing command: %s %c %s %s\n", GUID, command, arg1, payload);
    switch(command)
    {
    case 'a':
        f = open_file(arg1);
        fprintf(f, "%s\n", payload);
        fclose(f);
        int nbytes = write(sockfd,"ack",3);
        if (nbytes < 0) error("ERROR writing to socket");

        int i;
        for (i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET (i, &subscriber_fd_set))
            {
                char message[512];
                //memset(message, 0, sizeof(message));
                sprintf(message, "New Update: %s %s", GUID, payload);
                printf("Sending on %d: %s\n", i, message);
                nbytes = write(i,message, strlen(message));
                if (nbytes < 0) {
                    //error("ERROR writing to socket");
                    printf("Socket closed: %d\n", i);
                    FD_CLR(i, &subscriber_fd_set);
                }
            }
        }
        break;
    default:
        printf("Unknown Command: %c\n", command);
        break;
    }
}






int
read_from_client (int filedes)
{
    char buffer[MAXMSG];
    int nbytes;

    nbytes = read (filedes, buffer, MAXMSG);
    if (nbytes < 0)
    {
        /* Read error. */
        perror ("read");
        exit (EXIT_FAILURE);
    }
    else if (nbytes == 0) {
        /* End-of-file. */
        return -1;
    }
    else
    {
        /* Data read. */
        buffer[nbytes] = '\0'; // terminate string
        do_command(buffer, filedes);
        return 0;
    }
}







void cleanup()
{
    //fprintf(stderr, "Cleaning up...   ");
    close(sock_incoming_subscribers);
    close(sock_incoming_publishers);
}

int
main (void)
{
    signal(SIGPIPE, SIG_IGN); // disable SIGPIPE signal to allow recovery from write to closed socket
    atexit(cleanup);

    int i;
    struct sockaddr_in clientname;
    socklen_t clilen;

    /* Create the socket and set it up to accept connections. */
    sock_incoming_subscribers = make_socket(PORT);
    sock_incoming_publishers = make_socket(PORT+1);
    if (listen(sock_incoming_subscribers, 5) < 0)
    {
        perror ("Failed to listen sock_subscribers");
        exit (EXIT_FAILURE);
    }

    if (listen(sock_incoming_publishers, 5) < 0)
    {
        perror ("Failed to listen sock_publishers");
        exit (EXIT_FAILURE);
    }

    /* Initialize the sets of sockets. */
    FD_ZERO (&listening_fd_set);
    FD_ZERO (&subscriber_fd_set);
    FD_ZERO (&publisher_fd_set);


    printf("Entering main loop...\n");

    while (1)
    {

        // update set of sockets for listening. Don't listen for already connected subscribers:
        listening_fd_set = publisher_fd_set;
        FD_SET(sock_incoming_subscribers, &listening_fd_set);
        FD_SET(sock_incoming_publishers, &listening_fd_set);

        /* Block until input arrives on one or more active sockets. */
        if (select (FD_SETSIZE, &listening_fd_set, NULL, NULL, NULL) < 0)
        {
            perror ("select");
            exit (EXIT_FAILURE);
        }

        printf("After select\n");

        /* Service all the sockets with input pending. */
        for (i = 0; i < FD_SETSIZE; ++i)
            if (FD_ISSET (i, &listening_fd_set))
            {
                printf("ISSET in listening %d\n", i);
                if (i == sock_incoming_subscribers)
                {
                    /* Connection request on subscriber socket. */
                    clilen = sizeof (clientname);
                    int newsocket = accept (sock_incoming_subscribers,
                                  (struct sockaddr *) &clientname,
                                  &clilen);
                    if (newsocket < 0)
                    {
                        perror ("Failed to accept subscriber");
                        exit (EXIT_FAILURE);
                    }
                    printf (ANSI_COLOR_BLUE "Subscriber connection %d from host %d, port %hd.\n" ANSI_COLOR_RESET,
                             newsocket,
                             inet_ntoa (clientname.sin_addr),
                             ntohs (clientname.sin_port));
                    FD_SET (newsocket, &subscriber_fd_set);
                }
                else if (i == sock_incoming_publishers)
                {
                    /* Connection request on publisher socket. */
                    int newsocket;
                    clilen = sizeof (clientname);
                    newsocket = accept (sock_incoming_publishers,
                                  (struct sockaddr *) &clientname,
                                  &clilen);
                    if (newsocket < 0)
                    {
                        perror ("Failed to accept publisher");
                        exit (EXIT_FAILURE);
                    }
                    printf (ANSI_COLOR_BLUE "Publisher connection %d from host %d, port %hd.\n" ANSI_COLOR_RESET,
                             newsocket,
                             inet_ntoa (clientname.sin_addr),
                             ntohs (clientname.sin_port));
                    FD_SET (newsocket, &publisher_fd_set);
                }
                else
                {
                    /* Data arriving on an already-connected publisher socket */
                    if (read_from_client (i) < 0)
                    {
                        close (i);
                        FD_CLR (i, &publisher_fd_set);
                    }
                }
            }
    }
}
