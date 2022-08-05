Bloțiu Mihnea-Andrei - 323CA
Protocoale de comunicatie - Tema 2
Aplicație client-server TCP și UDP pentru gesionarea mesajelor

Ideea acestei teme a fost implementarea unei aplicații client-server care să permită gestionarea
mesajelor. În cele ce urmează voi explica cum am implementat toate cerințele.

Proiectul este împărțit în:
 - helpers.h - Macro-ul DIE preluat din scheletele de laborator pentru a trata situațiile
 neprevăzute;
 - structures.h - structurile create de mine, folosite pe parcursul programului;
 - subscriber.cpp - programul pentru un client TCP;
 - server.cpp - programul pentru server-ul ce ascultă atât TCP cât și UDP;
 - readme.txt - flow-ul programului care este completat în detaliu de comentariile din cod;
 - Makefile
 
structures.h:
 - tcp_client - conține datele unui client. Id-ul, adresa IP și port-ul sunt preluate de la
 tastatură, iar socket-ul este adăugat în momentul în care se dă accept pe server;
 - udp_message - structura ce încadrează mesajele pe care serverul le trimite la clienții TCP;
 - udp_message_received - structura ce parsează mesajele venit la server de la clienții UDP;
 
 subscriber.cpp:
 - De fiecare dată când se crează un client nou, instanțiem o structură nouă, corespunzătoare,
 cu datele acestuia, iar apoi instanțiem un socket de tip TCP pentru acesta;
 - Programul efectiv este practic o buclă infinită în care se multiplexează între a primi un
 mesaj de la server sau a introduce o comandă de la tastatură. Practic, clientul așteaptă una
 dintre aceste variante;
 - În cazul în care se citește de la tastatură, contează primul cuvânt al comenzii. Dacă acesta
 este "exit", atunci îi transmitem serverului faptul că acest client se va deconecta și ieșim
 din buclă;
 - Altfel, dacă este subscribe sau unsubscribe, atunci trimitem server-ului mesajele în întregime
 cu topic-urile la care se dorește a se abona/dezabona. Orice altă comandă este ignorată, dar este
 și anunțat faptul că acea comandă nu există;
 - Transmiterea mesajului se face în următorul fel pentru a eficientiza cât mai mult transmiterea
 de date: se transmite un int ce reprezintă dimensiunea comenzii în întregime, iar apoi se
 transmite comanda respectivă;
 - În cazul în care se primește de la server, acesta poate să îi transmită client-ului să se
 închidă, sau să facă forward la un mesaj primit de la UDP. În funcție de tipul mesajului primit
 de la server se face afișarea corespunzătoare;
 - În acest caz, primirea mesajului se face printr-o structură udp_message deoarece în cel mai rău
 caz, dimensiunea payload-ului este de 1500 de octeți;
 
 server.cpp:
 - Când se inițializează serverul pentru prima dată se crează un socket activ pentru conexiunea
 cu clienții UDP și un socket pasiv pentru conexiunea cu clienții TCP; Practic și acest program
 este o buclă infinită în care se așteaptă primirea unui mesaj de la unul dintre clienții TCP 
 sau UDP sau de la tastatură;
 - Dacă în această iterație, s-a citit de la tastatură, singura comandă posibilă este cea de exit
 moment în care transmitem tuturor clienților conectați în acel moment faptul că ar trebui să
 se închidă (folosim vectorul de clienți, "clients"), iar apoi închidem și server-ul;
 - Dacă s-a primit de pe socket-ul de UDP, atunci, recepționăm mesajul și îl trimitem mai departe
 tuturor clienților TCP care sunt abonați la acel topic;
 - Dacă s-a primit de pe socket-ul de TCP, înseamnă că un nou client vrea să se conecteze, așa că
 îl acceptăm și adăugăm socket-ul său în set-ul de read;
 - Altfel, înseamnă că s-a primit mesaj de la unul dintre clienții de TCP deja conectați cu
 server-ul. În acest sens, verificăm dacă client-ul există deja în lista de clienți, dacă nu există
 atunci, dacă nu mai există un alt client cu același ID, îl adăugăm, altfel îi spunem să se închidă
 deoarece nu putem avea doi clienți cu același id;
 - Dacă clientul există deja înseamnă că ne transmite o comandă de subscribe/unsubscribe. În cazul
 în care este un subscirbe, atunci adăugăm în map-ul "topics_map" asocierea (socket, topic la care)
 s-a abonat și în map-ul store_and_forw_map asocierea (id, pair(topic, sf)) pe care am folosit-o
 în implementarea de store and forward;
 - În cazul de unsubscribe se realizează operația inversă celei menționate anterior;
 - De asemenea, dacă un client s-a deconectat, server-ul trebuie să știe acest lucru, deci îl
 scoatem din lista de clienți, iar socket-ul său este scos din set-ul de read și închis.
 - În final, la fiecare iterație se încearcă trimiterea mesajelor care nu au putut fi trimise
 inițial deoarece un client cu store and forward pe un topic era deconectat. Practic așteptăm
 conectarea lui pentru a îi putea trimite toate mesajele care îi erau destinate, dar nu au putut
 fi trimise;
 
 Referințe:
 
 - Laboratorul 6 - protocolul UDP - https://ocw.cs.pub.ro/courses/pc/laboratoare/06;
 - Laboratorul 8 - TCP și multiplexare I/O - https://ocw.cs.pub.ro/courses/pc/laboratoare/08.
 
