In rezolvarea temei, am utilizat flowul din laboratorul 7 pentru implementarea atat
subscriberului si serverului, cat si pentru functiile de transmitere(send_all si recv_all).

Am utilizat C++ pentru a avea acces la implementari pentru map si vector care sa usureze implementarea.

Descrierea aplicatiei:
    Avem ca "actori" principali in cursul programului serverul si subscriberi.
    Serverul poate primi 4 tipuri de input:
        STDIN - comanda de "exit" duce la inchiderea serverului si subscriberilor,
        orice alta comanda de la tastatura este ignorata

        TCP CONNECT - dupa cererea de conectare, subscriberul va trimite si id-ul, daca
        id-ul este deja utilizat de un subscriber online, serverul va cere intreruperea 
        rulari noului subscriber

        UDP MESSAGE - serverul va trimite spre subscriberii abonati la acel topic pachetul udp
        la care ataseaza ip-ul si portul userului udp.

        TCP REQUEST - serverul poate primi de la un subscriber cereri de abonare, dezabonare si de iesire.
        Modul in care sunt implementate structurile utilizate este descris mai jos

    Clientul poate primi 2 tipuri de input:
        STDIN - cereri de abonare, dezabonare si exit care sunt transformate in pachete descrise mai jos
        si trimise catre server ca TCP REQUEST.

        UDP MESSAGE - pachete udp care sunt parasate in functie de tip, cum este descris in enuntul temei.

    Implementari utilizate:
        Structura Client - prezenta in server, este formata dintr-un socket, un map cu topicuri si un vector cu wildcards.
        La gestionarea TCP REQUEST, cererile de abonare si dezabonare devin operatii de adaugare si stergere pe mapuri si vectori.
        Clienti sunt tinuti intr-un map, avand drept chei id-urile, pentru a gestiona usor logarea de clienti noi.
        La deconectarea unui client, socketul lui devine -1 pentru a marca ca este inactiv.
        Am ales map pentru topicuri pentru a avea complexitate temporala pentru acces si adaugare constanta, pentru ca
        pentru topic se gaseste doar o intrare.
        Wildcardurile sunt un vector deoarece trebuiesc parcurse pana se gaseste unul care sa se potriveasca.

        Structura Req_Packet - formata dintr-un camp command(un enum care descrie operatia: SUB, UNSUB si EXIT) si un camp topic.
        Comenzile subscriberilor de abonare, dezabonare si iesire sunt transformate de client in aceste pachete si transmise serverului,
        unde se fac operatii in functie de comanda pe topic(la exit, topicul este null, doar se delogheaza subscriberul).

        Structra ExtendedUDP - este un fel de wrapper in jurul unui pachet udp, care contine portul si adresa ip a clientului udp. 
        Campul core este o structura UDP_Packet care contin topicul, type(un enum cu tipurile specificate) si adresa ip.

Descrierea protocolului peste TCP:
    Protocolul este caracterizt de un header, TCPHeader, cu 2 campuri: lungimea mesajului si purpose.
    Purpose este de 4 tipuri, corespunzator tipurilor de pachete pe care le manipuleaza serverul si subscriberul:
        DISPLAY - corespunde pachetelor cu ExtendedUDP, trimise de server catre subscriberi pentru a fi afisate.

        COMMAND - corespund pachetelor Req_Packet, comenzile transimise de catre subscriberi catre server

        ID - este id-ul trimis de subscriber catre server, sub forma de string

        QUIT - este un pachet special, headerul sune ca segmentul de date are lungimea 0. Este folsosit
        pentru ca serverul sa semnaleze subscriberului sa se inchida(fie atunci cand se inchide serverul, fie cand
        subscriberul are un id folosit de altcineva)

    Implementarile de send_tcp si recv_tcp, specifice protocolului, folosesc la baza send_all si recv_all care trimit
    un numar fix de octeti.
    send_tcp - trimite prima oara headerul, apoi payloadul
    recv_tcp - citeste headerul, iar in functie de tipul si de lungimea descrisa in header, citeste si returneza tipul de pachet
    primit(Pentru pachetele QUIT returneaza NULL).

Dificultati:
    Au aparut probleme in toate directile, de la managerierea pollingului(de aceea de exemplu num_cnnect este contorizat la final), pana la
    inchiderea subscriberilor in anumite circumstante(aici apare tipul de pachet QUIT).