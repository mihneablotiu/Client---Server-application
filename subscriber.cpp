#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <math.h>
#include "structures.h"
#include "helpers.h"

/* Funcție folosită pentru a iniția socket-ul de TCP pe care client-ul îl folosește pentru a 
se conecta la server */
void initiateTCPSocket (int &sockfd, struct sockaddr_in &serv_addr, tcp_client &new_tcp_client) {
    DIE((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0, "TCP socket error\n");

    // Dezactivam algoritmul lui Nagle pentru socket-ul TCP
    int flag = 1;
    DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0, "Nagle error\n");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = new_tcp_client.ip_address;
    serv_addr.sin_port = new_tcp_client.port;

    // Conectam socket-ul la server
    DIE(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0, "Connect error\n");

    // Trimitem datele clientului la server
    DIE(send(sockfd, &new_tcp_client, sizeof(tcp_client), 0) < 0, "Client information send error\n");
}

int main (int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != 4, "Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n");

    // Realizam noul client cu informatiile primite in linia de comanda
    tcp_client new_tcp_client;
    memset(&new_tcp_client, 0, sizeof(tcp_client));
    memcpy(new_tcp_client.id, argv[1], strlen(argv[1]));
    DIE(inet_aton(argv[2], &new_tcp_client.ip_address) == 0, "Inet error");
    new_tcp_client.port = htons(atoi(argv[3]));

    // Vrem sa multiplexam intre citirea de la tastatura sau primirea unui mesaj de la server
    fd_set readFds, tmpFds;

    // Ștergem în întregime mulțimea de descriptori de fișiere set
    FD_ZERO(&readFds);
    FD_ZERO(&tmpFds);

    char buffer[BUFLEN];
    int sockfd;
    struct sockaddr_in serv_addr;

    // Inițiem socket-ul de TCP
    initiateTCPSocket(sockfd, serv_addr, new_tcp_client);

    // Putem citi de pe socketul de TCP sau de la tastatura
    FD_SET(sockfd, &readFds);
    FD_SET(STDIN_FILENO, &readFds);

    while (true) {
        tmpFds = readFds;

        // Vedem cine a scris/de unde am citit în această iterație
        DIE(select(sockfd + 1, &tmpFds, NULL, NULL, NULL) < 0, "Select error\n");
        if (FD_ISSET(STDIN_FILENO, &tmpFds)) {
            // Citim de la tastatura
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);

            // Buffer pentru a observa cât am citit
            int buffer_length = strlen(buffer);

            /* Verificam dacă nu cumva vrem să închidem clientul. Dacă da, atunci
            trebuie să anunțăm și serverul că acest client s-a deconectat */
                if (strncmp(buffer, "exit", 4) == 0) {
                // Trimitem lungimea comenzii și comanda în sine de acea lungime
                DIE(send(sockfd, &buffer_length, sizeof(int), 0) < 0, "Length send error");
                DIE(send(sockfd, &buffer, buffer_length, 0) < 0, "Subscribe send error");
                break;
            }

            /* Verificăm dacă vrem să ne abonăm la un anumit topic. Dacă da, atunci
            trimitem lungimea comenzii, comanda în întregime și afișăm acest lucru
            Analog pentru operația inversă. În cazul în care se oferă o comandă 
            necunoscută, atunci o ignorăm, dar anunțăm acest lucru*/
            if (strncmp(buffer, "subscribe", 9) == 0) {
                DIE(send(sockfd, &buffer_length, sizeof(int), 0) < 0, "Length send error");
                DIE(send(sockfd, &buffer, buffer_length, 0) < 0, "Subscribe send error");

                printf("%s\n", "Subscribed to topic.");

            } else if (strncmp(buffer, "unsubscribe", 11) == 0) {
                DIE((send(sockfd, &buffer_length, sizeof(int), 0)) < 0, "Length send error");
                DIE((send(sockfd, &buffer, buffer_length, 0) < 0), "Unsubscribe send error");
                
                printf("%s\n", "Unsubscribed from topic.");
            } else {
                fprintf(stderr, "%s", "Unknown command\n");
                continue;
            }
        }

        if (FD_ISSET(sockfd, &tmpFds)) {
            /* Primim de la server. Știm că întotdeauna vom primi o structură 
            de tip UDP message */
            udp_message new_message;
            memset(&new_message, 0, sizeof(udp_message));
            DIE((recv(sockfd, &new_message, sizeof(udp_message), 0) < 0), "Message receive error");

            // Vedem dacă nu cumva serverul i-a spus clientului să se închidă.
            if (strncmp(new_message.payload, "exit", 4) == 0) {
                break;
            }

            /* In funcție de tipul de date primit facem afișărea corespunzătoare conform cerinței
            sau ieșim forțat dacă cumva primit vreun tip de date la care nu ne așteptam */
            if (new_message.data_type == 0) {
                char *sign = new_message.payload;
                uint32_t number = * (uint32_t *)(new_message.payload + 1);
                number = htonl(number);
                if (*sign == 1) {
                    printf("%s:%hu - %s - %s - -%d\n", inet_ntoa(new_message.ip_address), new_message.port, 
                        new_message.topic, "INT", number);
                } else {
                    printf("%s:%hu - %s - %s - %d\n", inet_ntoa(new_message.ip_address), new_message.port, 
                        new_message.topic, "INT", number);
                }

            } else if (new_message.data_type == 1) {
                printf("%s:%hu - %s - %s - %.2f\n", inet_ntoa(new_message.ip_address), new_message.port, 
                        new_message.topic, "SHORT_REAL", (float) htons(* (uint16_t *)new_message.payload) / 100);

            } else if (new_message.data_type == 2) {
                char *sign = new_message.payload;
                uint32_t fullNumber = htonl(*(uint32_t *)(new_message.payload + 1));
                uint8_t power = *(uint8_t *)(new_message.payload + 5);

                if (*sign == 0) {
                    printf("%s:%hu - %s - %s - %f\n", inet_ntoa(new_message.ip_address), new_message.port, 
                        new_message.topic, "FLOAT", fullNumber * pow(10, -power));
                } else if (*sign == 1) {
                    printf("%s:%hu - %s - %s - %f\n", inet_ntoa(new_message.ip_address), new_message.port, 
                        new_message.topic, "FLOAT", -(fullNumber * pow(10, -power)));
                }
                
            } else if (new_message.data_type == 3) {
                printf("%s:%hu - %s - %s - %s\n", inet_ntoa(new_message.ip_address), new_message.port, 
                        new_message.topic, "STRING", new_message.payload);
            } else {
                fprintf(stderr, "%s\n", "Unknown data type\n");
                exit(1);
            }
        }

    }

    close(sockfd);
    return 0;
}