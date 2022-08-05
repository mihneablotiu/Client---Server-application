#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <bits/stdc++.h>
#include "structures.h"
#include "helpers.h"

#define MAX_CLIENTS_NO 100

/* Funcție folosită pentru a iniția socket-ul de UDP pe care clienții de UDP îl folosesc pentru a 
se conecta la server */
void initiateUDPSocket (int &udpsocket, char *argv[], struct sockaddr_in &serv_udp_addr) {
    DIE((udpsocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0, "UDP socket error\n");

    memset(&serv_udp_addr, 0, sizeof(serv_udp_addr));
    serv_udp_addr.sin_family = AF_INET;
    serv_udp_addr.sin_addr.s_addr = INADDR_ANY;
    serv_udp_addr.sin_port = htons(atoi(argv[1]));

    // Facem bind socket-ului de UDP
    DIE(bind(udpsocket, (const struct sockaddr *) &serv_udp_addr, sizeof(serv_udp_addr)) < 0, "UDP binding error\n");
}

/* Funcție folosită pentru a iniția socket-ul de TCP pe care clienții de TCP îl folosesc pentru a 
se conecta la server */
void initiateTCPSocket (int &tcpsocket, char *argv[], struct sockaddr_in &serv_tcp_addr) {
    DIE((tcpsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0, "TCP socket error\n");

    // Dezactivam algoritmul lui Nagle pentru socket-ul TCP
    int flag = 1;
    DIE(setsockopt(tcpsocket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0, "Nagle error\n");

    memset(&serv_tcp_addr, 0, sizeof(serv_tcp_addr));
    serv_tcp_addr.sin_family = AF_INET;
    serv_tcp_addr.sin_addr.s_addr = INADDR_ANY;
    serv_tcp_addr.sin_port = htons(atoi(argv[1]));

    // Facem bind socket-ului de TCP
    DIE(bind(tcpsocket, (const struct sockaddr *) &serv_tcp_addr, sizeof(serv_tcp_addr)) < 0, "TCP binding error\n");

    // Ascultam pe socket-ul pasiv de TCP
    DIE(listen(tcpsocket, MAX_CLIENTS_NO) < 0, "TCP listen error\n");
}

/* Funcție folosită pentru a vedea dacă există deja sau nu un client în vectorul de clienți conectați */
bool check_existing_id (std::vector<tcp_client> &clients, tcp_client &current_client) {
    for (long unsigned int i = 0; i < clients.size(); i++) {
        if (strcmp(clients[i].id, current_client.id) == 0) {
            return true;
        }
    }

    return false;
}

/* Funcție folosită pentru a vedea dacă există deja sau nu un anumit socket atribuit unui client din
vectorul de clienți conectați */
bool check_existing_socket (std::vector<tcp_client> &clients, int socket) {
    for (long unsigned int i = 0; i < clients.size(); i++) {
        if (clients[i].socket == socket) {
            return true;
        }
    }

    return false;
}

/* Funcție folosită pentru a trimite un mesaj clienților de TCP conectați în acest moment pentru a se
închide (de exemplu în cazul în care server-ul primeșțe comanda exit și trebuie să se închidă și toți
clienții) */
void sendTCPExitMessageToAllClients (std::vector<tcp_client> &clients) {
    udp_message exit_message;
    memset(&exit_message, 0, sizeof(udp_message));
    strcpy(exit_message.payload, "exit");

    for (long unsigned int i = 0; i < clients.size(); i++) {
        DIE(send(clients[i].socket, &exit_message, sizeof(udp_message), 0) < 0, "Exit message send error");
        printf("Client %s disconnected.\n", clients[i].id);
    }
}

/* Funcție folosită pentru a trimite un mesaj de exit unui singur client de TCP pe un anumit socket
(de exemplu atunci când un client încearcă să se conecteze cu un ID deja existent) */
void sendTCPExitMessageToClient (int socket) {
    udp_message exit_message;
    memset(&exit_message, 0, sizeof(udp_message));
    strcpy(exit_message.payload, "exit");

    DIE(send(socket, &exit_message, sizeof(udp_message), 0) < 0, "Exit message send error");

}

/* Funcție care verifică dacă două mesaje UDP sunt echivalente, adică dacă au același conținut */
bool udpMessageCompare (udp_message &first, udp_message &second) {
    if (first.data_type == second.data_type && strncmp(first.topic, second.topic, 50)
        && strncmp(first.payload, second.payload, 1500)) {
            return true;
    }

    return false;
}

/* Funcție pe care o folosim pentru a trimite un mesaj către toți clienții TCP care sunt abonați la un anumit
topic sau pentru a îl stoca în cazul în care nu îl putem trimite acum */
void sendUDPMessageToAllClients 
(std::map<std::string, std::vector<std::pair<std::string, int>>> &store_and_forw_map,
std::vector<std::pair<udp_message, std::vector<std::string>>> &to_send_vector, udp_message_received &original_message, 
struct sockaddr_in &udp_client_info, std::vector<tcp_client> &clients, std::map<int, std::vector<std::string>> &topics_map) {
    /* Realizăm de la 0 un mesaj pe care o să îl trimitem pe baza celui pe care l-am primit
    de la clienții de UDP, adică adăugăm IP-ul și port-ul*/
    udp_message new_message;
    memset(&new_message, 0, sizeof(udp_message));

    memcpy(new_message.topic, original_message.topic, 50);
    memcpy(new_message.payload, original_message.payload, 1500);
    new_message.data_type = original_message.data_type;

    new_message.port = ntohs(udp_client_info.sin_port);
    new_message.ip_address = udp_client_info.sin_addr;

    /* Pentru fiecare intrare din map-ul de store and forward, încercăm să vedem dacă există client-ul
    cu id-ul respectiv conectat în acest moment ca să îi trimitem mesajul */
    for (auto it = store_and_forw_map.begin(); it != store_and_forw_map.end(); it++) {
        bool found = false;
        for (auto client_it = clients.begin(); client_it != clients.end(); client_it++) {
            if (std::string(client_it->id) == it->first) {
                std::vector<std::string> search_vector = topics_map[client_it->socket];
                found = true;
                if (std::find(search_vector.begin(), search_vector.end(), original_message.topic) != search_vector.end()) {
                    DIE(send(client_it->socket, &new_message, sizeof(udp_message), 0) < 0, "Message send error");
                }

                break;
            }
        }

        /* Dacă respectiv-ul client nu este conectat în acest moment, dar are store and forward pe respectivul 
        subiect, trebuie să îl păstrăm pentru mai departe. Trebuie să facem asta pentru toți clienții care au 
        store and forward, dar nu sunt conectați în acest moment*/
        if (found == false) {
            std::vector<std::pair<std::string, int>> pairs_vector = it->second;
            for (auto pairs_it = pairs_vector.begin(); pairs_it != pairs_vector.end(); pairs_it++) {
                if (pairs_it->first == std::string(original_message.topic)) {
                    if (pairs_it->second == 1) {
                        for (auto pairs_vector_it = to_send_vector.begin(); pairs_vector_it != to_send_vector.end(); pairs_vector_it++) {
                            if (udpMessageCompare(pairs_vector_it->first, new_message)) {
                                found = true;
                                pairs_vector_it->second.push_back(it->first);
                            } 
                        }

                        if (found == false) {
                            std::vector<std::string> new_entry;
                            new_entry.push_back(it->first);
                            to_send_vector.push_back(std::make_pair(new_message, new_entry));
                        }
                    }
                }
            }
        }
    }

}

/* Funcție folosită la fiecare iterație pentru a încerca să trimită un mesaj din "coadă" clienților
care s-au conectat în această iterație, dar aveau store and forward din iterațiile anterioare */
void tryToSend
(std::vector<std::pair<udp_message, std::vector<std::string>>> &to_send_vector, std::vector<tcp_client> &clients) {
    for (auto vector_it = to_send_vector.begin(); vector_it != to_send_vector.end(); ) {
        for (auto ids_it = vector_it->second.begin(); ids_it != vector_it->second.end(); ) {
            bool found = false;
            for (auto clients_it = clients.begin(); clients_it != clients.end(); clients_it++) {
                if (std::string(clients_it->id) == *ids_it) {
                    DIE(send(clients_it->socket, &vector_it->first, sizeof(udp_message), 0) < 0, "Message send error");
                    found = true;
                    break;
                }
            }

            if (found == true) {
                ids_it = vector_it->second.erase(ids_it);
            } else {
                ids_it++;
            }
        }

        if (vector_it->second.size() == 0) {
            vector_it = to_send_vector.erase(vector_it);
        } else {
            vector_it++;
        }
    }
}



int main (int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != 2, "Usage: ./server <PORT_DORIT>\n");

    fd_set readFds, tmpFds; // Mulțimea de citire folosita in select
    std::vector<tcp_client> clients; // Vector de clienți conectați la un anumit moment de timp
    std::map<int, std::vector<std::string>> topics_map; /* Asociere între un socket și topic-urile
                                                        la care este conectat client-ul de pe acel socket */

    // Asociere între un ID al unui client și un vector de perechi (topic, sf)
    std::map<std::string, std::vector<std::pair<std::string, int>>> store_and_forw_map;

    // "Coadă" de mesaje ce trebuie trimise către mai multe Id-uri de utilizator
    std::vector<std::pair<udp_message, std::vector<std::string>>> to_send_vector;
    
    struct sockaddr_in tcp_client_addr, udp_client_addr;

    // Ștergem în întregime mulțimea de descriptori de fișiere set
    FD_ZERO(&readFds);
    FD_ZERO(&tmpFds);

    // Realizam socket-ul pentru conexiunile UDP
    int udpsocket;
    struct sockaddr_in serv_udp_addr;
    initiateUDPSocket(udpsocket, argv, serv_udp_addr);

    // Realizam socket-ul pentru conexiunile TCP
    int tcpsocket;
    struct sockaddr_in serv_tcp_addr;
    initiateTCPSocket(tcpsocket, argv, serv_tcp_addr);

    /* Ne așteptăm să primim pe oricare dintre cele două socket-uri menționate anterior 
    sau să citim de la tastatură */
    FD_SET(udpsocket, &readFds);
    FD_SET(tcpsocket, &readFds);
    FD_SET(STDIN_FILENO, &readFds);
    
    int fdmax = std::max(tcpsocket, udpsocket);

    while (true) {
        tmpFds = readFds;

        // Vedem pe ce socket-uri s-a scris în această iterație
        DIE (select(fdmax + 1, &tmpFds, NULL, NULL, NULL) < 0, "Select error\n");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmpFds)) {
                // Dacă s-a scris de la tastatura
                if (i == STDIN_FILENO) {
                    char buffer[BUFLEN];
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);

                    /* Vedem dacă nu cumva vrem să închidem server-ul, în acest caz trebuie să
                    închidem toți clienții conectați la acel moment */
                    if (strncmp(buffer, "exit", 4) == 0) {
                        sendTCPExitMessageToAllClients(clients);
                        return 0;
                    } else {
                        fprintf(stderr, "%s", "Invalid command\n");
                        break;
                    }
                } else if (i == udpsocket) {
                    /* Dacă un client udp a trimis vreun mesaj, atunci trimitem acest mesaj tuturor 
                    clienților abonați la topic-ul din mesaj */
                    udp_message_received newmessage;
                    memset(&newmessage, 0, sizeof(udp_message_received));
                    socklen_t clientlength = sizeof(udp_client_addr);

                    // Preluăm mesajul și îl trimitem mai departe
                    DIE((recvfrom(udpsocket, &newmessage, sizeof(udp_message_received), 0, (struct sockaddr *) &udp_client_addr,
                                    &clientlength)) < 0, "Receive from UDP failed");

                    sendUDPMessageToAllClients(store_and_forw_map, to_send_vector, newmessage, udp_client_addr, clients, topics_map);

                } else if (i == tcpsocket) {
                    // A venit o cerere de pe socket-ul inactiv de tcp, trebuie sa o acceptam
                    socklen_t clientlength = sizeof(tcp_client_addr);
                    int newsockfd = accept(tcpsocket, (struct sockaddr *) &tcp_client_addr, &clientlength);
                    DIE((newsockfd < 0), "Accept error");

                     // Dezactivam algoritmul lui Nagle pentru socket-ul TCP
                    int flag = 1;
                    DIE(setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0, "Nagle error\n");

                    // Adăugam noul socket intors de accept() la multimea de read
                    FD_SET(newsockfd, &readFds);

                    // Actualizam filedescriptor-ul maxim
                    fdmax = std::max(fdmax, newsockfd);
                } else {
                    /* Înseamnă că unul dintre clienții deja conectați prin TCP transmite ceva. Dacă nu există în
                    vectorul de clienți deja conectați înseamnă că își transmite datele*/
                    if (!check_existing_socket(clients, i)) {
                        tcp_client newclient;
                        DIE(recv(i, &newclient, sizeof(tcp_client), 0) < 0, "Data receive error");
                        newclient.socket = i;

                        /* Dacă mai există vreun client cu numele său atunci nu îl lăsăm să se conecteze și 
                        trimitem mesaj să se închidă. Altfel, îl adăugăm în vectorul de clienți și afișăm conexiunea*/
                        if (check_existing_id(clients, newclient) == true) {
                            printf("Client %s already connected.\n", newclient.id);
                            sendTCPExitMessageToClient(i);
                            FD_CLR(i, &readFds);
                        } else {
                            printf("New client %s connected from %s:%hd\n", newclient.id, inet_ntoa(newclient.ip_address), newclient.port);
                            clients.push_back(newclient);
                        }

                    } else {
                        /* Înseamnă că actualul client este conectat de mai mult timp. Deci primim lungimea mesajului
                        pe care îl trimite și mesajul de lungimea respectivă */
                        int buffer_length;
                        DIE(recv(i, &buffer_length, 4, 0) < 0, "Buffer length recv error");

                        char buffer[buffer_length];
                        DIE(recv(i, buffer, buffer_length, 0) < 0, "Buffer recv error");
                    
                        // Extragem primul cuvânt din comandă și verificăm ce anume se dorește
                        char *token = strtok(buffer, " ");

                        /* Dacă respectivul client dorește să facă subscribe, atunci verificăm dacă
                        este primul topic la care se abonează și realizăm o intrare nouă în map, altfel
                        actualizăm lista sa de topic-uri adăugându-l pe cel nou*/
                        if (strncmp(token, "subscribe", 9) == 0) {
                            token = strtok(NULL, " ");
                            if (topics_map.count(i)) {
                                topics_map[i].push_back(token);
                            } else {
                                std::vector<std::string> new_topics_vector;
                                new_topics_vector.push_back(token);
                                topics_map[i] = new_topics_vector;
                            }

                            /* De asemenea adăugăm și perechea corespunzătoare (topic, sf) pentru clientul
                            cu ID-ul curent (o vom folosi pentru partea de store and forward */
                            int sf = atoi(token + strlen(token) + 1);
                            for (unsigned long int j = 0; j < clients.size(); j++) {
                                if (clients[j].socket == i) {
                                    if (store_and_forw_map.count(std::string(clients[j].id))) {
                                        store_and_forw_map[clients[j].id].push_back(std::make_pair(token, sf));
                                    } else {
                                        std::vector<std::pair<std::string, int>> new_entry;
                                        new_entry.push_back(std::make_pair(token, sf));
                                        store_and_forw_map[clients[j].id] = new_entry;
                                    }
                                }
                            }

                        } else if (strncmp(token, "unsubscribe", 11) == 0) {
                            token = strtok(NULL, " ");

                            /* Dacă se vrea unsubscribe atunci scoatem de pe socket-ul curent topic-ul la care se vrea
                            să se dea unsubscribe */
                            for (auto vector_it = topics_map[i].begin(); vector_it != topics_map[i].end(); ) {
                                if (strncmp(token, (*vector_it).c_str(), strlen((*vector_it).c_str())) == 0) {
                                    vector_it = topics_map[i].erase(vector_it);
                                } else {
                                    vector_it++;
                                }
                            }

                            /* De asemenea o scoatem și din lista de store and forward */
                            for (unsigned long int j = 0; j < clients.size(); j++) {
                                if (clients[j].socket == i) {
                                    std::vector<std::pair<std::string, int>> current_vector = store_and_forw_map[clients[j].id]; 
                                    for (auto st_and_fw_it = current_vector.begin(); st_and_fw_it != current_vector.end(); ) {
                                        if ((*st_and_fw_it).first == std::string(token)) {
                                            st_and_fw_it = current_vector.erase(st_and_fw_it);
                                        } else {
                                            st_and_fw_it++;
                                        }
                                    }
                                }

                            }
                        } else if (strncmp(token, "exit", 4) == 0) {
                            /* Dacă client-ul a dat exit, atunci trebuie să îl scoatem din vectorul de clienți
                            conectați acum, să scoatem socket-ul din set-ul de read și să îl închidem */
                            for (auto j = clients.begin(); j != clients.end(); j++) {
                                if ((*j).socket == i) {
                                    printf("Client %s disconnected.\n", (*j).id);
                                    clients.erase(j);
                                    FD_CLR(i, &readFds);
                                    close(i);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* La fiecare iterație încercăm să trimitem mesaje pentru clienții care au store and forward 
        pe anumite topic-uri, dar au fost deconectați, iar acum s-au conectat */
        tryToSend(to_send_vector, clients);
    }

    close(udpsocket);
    close(tcpsocket);
    return 0;
}