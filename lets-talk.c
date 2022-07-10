#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include "list.h"

#define MAX_LEN 4000
#define ENCRYPTION_KEY 78

List *input_list;
List *output_list;

pthread_t input_t;
pthread_t send_t;
pthread_t receive_t;
pthread_t print_t;

sem_t empty1, mutex3, full1;
sem_t empty2, mutex4, full2;

bool flag1;
bool flag2;
bool online_state;

struct sockaddr_in servaddr, cliaddr;

void *input_info(void *sockfd)
{
    while(1)
    {
        sem_wait(&empty1);
        sem_wait(&mutex3);
        int n = 0;
        char buff1[MAX_LEN];
        bzero(buff1, sizeof(buff1));
        while ((buff1[n++] = getchar()) != '\n');
        buff1[n - 1] = '\0';
        List_prepend(input_list, buff1);
        sem_post(&mutex3);
        sem_post(&full1);
        if (strncmp(buff1, "!exit", 5) == 0)
        {
            sem_post(&full2);
            break;
        }
    }
    pthread_exit(NULL);
}

void *send_info(void *sockfd)
{
    while(1)
    {
        sem_wait(&full1);
        sem_wait(&mutex3);
        char *buff1 = List_first(input_list);
        List_free(input_list, List_remove(input_list));
        char temp[strlen(buff1)];
        strcpy(temp, buff1);
        for (int i = 0; i < strlen(buff1); i++)
            buff1[i] = (buff1[i] + ENCRYPTION_KEY) % 256;
        int sv = sendto(*((int *)sockfd), buff1, strlen(buff1), 0, 
            (struct sockaddr*)&cliaddr, sizeof(cliaddr));
        if (sv == -1){
            printf("Fail to send\n");
            fflush(stdout);
        }
        if (strncmp(temp, "!status", 7) == 0){
            sleep(1);
            if (!online_state){
                printf("Offline\n");
                fflush(stdout);
            }
        }
        sem_post(&mutex3);
        sem_post(&empty1);
        if (strncmp(temp, "!exit", 5) == 0)
        {
            sem_post(&full2);
            pthread_cancel(print_t);
            pthread_cancel(receive_t);
            break;
        }
    }
    pthread_exit(NULL);
}

void *receive_info(void *sockfd)
{
    while(1)
    {
        sem_wait(&empty2);
        sem_wait(&mutex4);
        char buff2[MAX_LEN];
        bzero(buff2, sizeof(buff2));
        socklen_t len = sizeof(servaddr);
        int rv = recvfrom(*((int *)sockfd), buff2, MAX_LEN, 0, 
            (struct sockaddr*)&servaddr, &len);
        if (rv == -1){
            printf("Fail to receive\n");
            fflush(stdout);
        }
        for (int i = 0; i < strlen(buff2); i++)
            buff2[i] = (buff2[i] + 256 - ENCRYPTION_KEY) % 256;
        if (strncmp(buff2, "!status", 7) == 0){
            char *temp = "Online";
            char temp2[strlen(temp)];
            for (int i = 0; i < strlen(temp); i++)
                temp2[i] = (temp[i] + ENCRYPTION_KEY) % 256;
            int sv = sendto(*((int *)sockfd), temp2, strlen(temp2), 0, 
                (struct sockaddr*)&cliaddr, sizeof(cliaddr));
            if (sv == -1){
                printf("Fail to send\n");
                fflush(stdout);
            }
        }
        List_prepend(output_list, buff2);
        sem_post(&mutex4);
        sem_post(&full2);
        if (strncmp(buff2, "!exit", 5) == 0)
        {
            sem_post(&full1);
            break;
        }
    }
    pthread_exit(NULL);
}

void *print_info(void *sockfd)
{
    while(1)
    {
        sem_wait(&full2);
        sem_wait(&mutex4);
        char *buff2 = List_first(output_list);
        List_free(output_list, List_remove(output_list));
        if (strncmp(buff2, "Online", 6) == 0){
            online_state = true;
        }
        if (strncmp(buff2, "!status", 7) != 0){
            printf("%s\n", buff2);
            fflush(stdout);
        }
        sem_post(&mutex4);
        sem_post(&empty2);
        if (strncmp(buff2, "!exit", 5) == 0)
        {
            sem_post(&full1);
            pthread_cancel(input_t);
            pthread_cancel(send_t);
            break;
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if (argc < 4){
        printf("Usage:\n  ./lets-talk <local port> <remote host> <remote port>\n");
        fflush(stdout);
        printf("Examples:\n  ./lets-talk 3000 192.168.0.513 3001\n  ./lets-talk 3000 some-computer-name 3001\n");
        fflush(stdout);
        exit(-1);
    }

    printf("Welcome to Lets-Talk! Please type your messages now.\n");
    fflush(stdout);

    input_list = List_create();
    output_list = List_create();

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1)
    {
        printf("SOCKET ERROR\n");
        fflush(stdout);
        close(sockfd);
        exit(0);
    }

    bzero(&servaddr, sizeof(servaddr));
    bzero(&cliaddr, sizeof(cliaddr));

    if (strncmp("localhost", argv[2], 9) == 0)
    {
        char host[MAX_LEN];
        struct addrinfo temp;
        bzero(&temp, sizeof(temp));
        struct addrinfo *addr;
        int get = getaddrinfo(argv[2], NULL, &temp, &addr);
        if (get != 0){
            printf("Fail to get address\n");
            fflush(stdout);
            exit(-1);
        }
        getnameinfo(addr->ai_addr, addr->ai_addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
        servaddr.sin_addr.s_addr = inet_addr(host);
    }
    else
    {
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        cliaddr.sin_addr.s_addr = inet_addr(argv[2]);
    }
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[1]));
    cliaddr.sin_port = htons(atoi(argv[3]));    

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("SOCKET BIND ERROR\n");
        fflush(stdout);
        close(sockfd);
        exit(0);
    }

    sem_init(&empty1, 0, 1);
    sem_init(&mutex3, 0, 1);
    sem_init(&full1, 0, 0);
    sem_init(&empty2, 0, 1);
    sem_init(&mutex4, 0, 1);
    sem_init(&full2, 0, 0);

    flag1 = true;
    flag2 = true;
    online_state = false;

    pthread_create(&input_t, NULL, input_info, &sockfd);
    pthread_create(&send_t, NULL, send_info, &sockfd);
    pthread_create(&receive_t, NULL, receive_info, &sockfd);
    pthread_create(&print_t, NULL, print_info, &sockfd);
    pthread_join(print_t, NULL);
    pthread_join(input_t, NULL);
    pthread_join(send_t, NULL);
    pthread_join(receive_t, NULL);

    sem_destroy(&empty1);
    sem_destroy(&mutex3);
    sem_destroy(&full1);
    sem_destroy(&empty2);
    sem_destroy(&mutex4);
    sem_destroy(&full2);

    close(sockfd);

    return 0;
}