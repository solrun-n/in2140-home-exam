#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>


#include "l2sap.h"

 // compute_checksum beregner checksum av rammen ved en XOR-operasjon
static uint8_t compute_checksum( const uint8_t* frame, int len ) {
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum ^= frame[i]; 
    }
    return checksum;
}


L2SAP* l2sap_create( const char* server_ip, int server_port ) {

    // socket() returnerer en file descriptor
    int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFD < 0) {
        printf("Couldn't create socket\n");
        exit(EXIT_FAILURE);
    }

    // Initialiserer pointer til struct L2SAP
    L2SAP* l2sap = malloc(sizeof(struct L2SAP));
    if (l2sap == NULL) {
        printf("Error mallocing L2SAP\n");
        exit(EXIT_FAILURE);
    }

    // Initialiserer struct sockaddr_in som skal inn i l2sap
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);


    int check = inet_pton(AF_INET, server_ip, &addr.sin_addr);
    if (check != 1) {
        printf("Couldn't convert to network address structure\n");
        free(l2sap);
        exit(EXIT_FAILURE);
    }

    // Tilordner variablene til socket (FD + adressen)
    l2sap->socket = socketFD;
    l2sap->peer_addr = addr;
    return l2sap;
}


void l2sap_destroy(L2SAP* client) {
    // Lukker socket og frigjør ressursene
    close(client->socket);
    free(client);
}


int l2sap_sendto( L2SAP* client, const uint8_t* data, int len ) {

    // Hvis datamengden er for stor (data + header overskrider rammestrl)
    if (len + L2Headersize > L2Framesize) {
        printf("Data exceeds frame size\n");
        return -1;
    }

    int socketFD = client->socket;
    struct sockaddr_in reciever = client->peer_addr;

    // Allokerer header på stacken 
    struct L2Header header;
    header.dst_addr = reciever.sin_addr.s_addr;
    header.len = htons((uint16_t)len + sizeof(L2Header));
    header.checksum = 0; // Checksum = 0 før den kalkuleres
    header.mbz = 0;

    // Oppretter en frame (buffer for header + data)
    // Rammen er begrenset til størrelsen av data
    int framesize = L2Headersize + len;
    printf("Framesize: %d\n", framesize);
    uint8_t* frame = malloc(framesize);
    if (frame == NULL) {
        printf("Error mallocing space for buffer\n");
        return -1;
    }

    // Kopierer først header på rammen og deretter dataen
    memcpy(frame, &header, L2Headersize);
    memcpy(frame+L2Headersize, data, len);

    // Kaller på hjelpefunksjon for å sette checksum
    // Beregnes ut fra hele rammen
    uint8_t cs = compute_checksum(frame, framesize);
    header.checksum = cs;

    // Oppdaterer headeren på frame etter checksum er beregnet
    memcpy(frame, &header, L2Headersize);

    // Sender melding (sender med hele bufferet, inkludert header)
    sendto(socketFD, frame, framesize, 0, (const struct sockaddr*)&reciever, sizeof(reciever));
    free(frame);
    return 1;
}


// Kaller recieve med evig venting (ingen timeout)
int l2sap_recvfrom( L2SAP* client, uint8_t* data, int len ) {
    return l2sap_recvfrom_timeout( client, data, len, NULL );
}


int l2sap_recvfrom_timeout( L2SAP* client, uint8_t* data, int len, struct timeval* timeout ) {

    // Nullstiller variabel som skal holde på file descriptor
    // og henter riktig FD fra klienten
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(client->socket, &fds);

    // Bruker select() for å overvåke endringer på sockets
    // Kun interessert i å lese, så setter writefds og exceptfds til null
    int check_activity = select(client->socket + 1, &fds, NULL, NULL, timeout);
    if (check_activity < 0) {
        printf("An error occured in select\n");
        return -1;
    } else if (check_activity == 0) {
        printf("Timeout waiting for data\n");
        return L2_TIMEOUT;

    // Hvis data er sendt og mottatt innen timeout:
    } else {

        // Lagrer adressestørrelse (for parameter)
        socklen_t address_length = sizeof(client->peer_addr);

        // recvfrom() returnerer en int (rammestørrelsen)
        int recv_len = recvfrom(client->socket, data, len, 0, (struct sockaddr*) &client->peer_addr, &address_length);

        if (recv_len < L2Headersize) {
            printf("Frame too short\n");
            return -1;
        }

        // recv_cs = mottatt checksum fra frame
        // correct_cs = kalkulert checksum basert på frame
        uint8_t recv_cs = data[L2Headersize-2];
        data[L2Headersize-2] = 0; // Setter checksum til 0 før beregning

        uint8_t correct_cs = compute_checksum(data, recv_len);
        if (recv_cs != correct_cs) {
            printf("Checksum not correct\n");
            return -1;
        }

        // Fjerne headeren 
        // Oppdaterer pointer til å peke på data etter header
        uint8_t* payload = data + L2Headersize;
        memcpy(data, payload, recv_len - L2Headersize); 

        return recv_len-L2Headersize;
    }
}