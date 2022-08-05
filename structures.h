/* Structură ce reține datele unui client TCP.
Aceasta este completată parțial în subscriber, iar apoi
socket-ul este atribuit de către server */
typedef struct {
    char id[10]; // ID-ul unic al fiecărui client
    in_addr ip_address; // Adresa de IP la care se conectează clientul
    in_port_t port; // Portul pe care se conectează clientul
    int socket; /* Socket-ul pe care server-ul l-a oferit prin accept
                clientului curent */
}tcp_client;

/* Structură folosită pentru a trimite un mesaj TCP venit
de la un client UDP. Adaugăm ip-ul și portul clientului
UDP pe lângă informațiile pe care le-am primit de la acesta*/
typedef struct {
    in_addr ip_address; // Adresa de IP a clientului UDP ce a trimis mesajul
    in_port_t port; // Portul pe care clientul UDP a trimis mesajul
    char topic[50]; /* Topic-ul mesajului pe care îl trimite clientul UDP, acesta are
                    mereu 50 de octeți */
    uint8_t data_type; // Tipul de date pe care îl conține payload-ul din mesaj
    char payload[1500]; // Payload-ul din mesaj, acesta are maxim 1500 de octeți
}udp_message;

/* Structură folosită pentru a primi un mesaj de tip UDP
de la un client UDP */
typedef struct {
    char topic[50]; /* Topic-ul mesajului pe care îl trimite clientul UDP, acesta are
                    mereu 50 de octeți */
    uint8_t data_type; // Tipul de date pe care îl conține payload-ul din mesaj
    char payload[1500]; // Payload-ul din mesaj, acesta are maxim 1500 de octeți
}udp_message_received;