#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "l2sap.h"

/* compute_checksum is a helper function for l2_sendto and
 * l2_recvfrom_timeout to compute the 1-byte checksum both
 * on sending and receiving and L2 frame.
 */
static uint8_t compute_checksum( const uint8_t* frame, int len ) {

    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum ^= frame[i]; // XOR-operasjon
    }
    return checksum;
}


L2SAP* l2sap_create( const char* server_ip, int server_port )
{
    // socket() returnerer en file descriptor
    int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFD < 0) {
        perror("Couldn't create socket");
        exit(EXIT_FAILURE);
    }

    // Initialiserer pointer til struct L2SAP
    L2SAP* l2sap = malloc(sizeof(struct L2SAP));
    if (l2sap == NULL) {
        perror("Error mallocing L2SAP");
        exit(EXIT_FAILURE);
    }

    // Initialiserer struct sockaddr_in som skal inn i l2sap
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);

    int check = inet_pton(AF_INET, server_ip, &addr.sin_addr);
    if (check != 1) {
        perror("Couldn't convert to network address structure");
        free(l2sap);
        exit(EXIT_FAILURE);
    }

    // Tilordner variablene til socket (FD + adressen)
    l2sap->socket = socketFD;
    l2sap->peer_addr = addr;
    return l2sap;
}

void l2sap_destroy(L2SAP* client)
{
    fprintf( stderr, "%s has not been implemented yet\n", __FUNCTION__ );
}

/* l2sap_sendto sends data over UDP, using the given UDP socket
 * sock, to a remote UDP receiver that is identified by
 * peer_address.
 * The parameter data points to payload that L3 wants to send
 * to the remote L3 entity. This payload is len bytes long.
 * l2_sendto must add an L2 header in front of this payload.
 * When the payload length and the L2Header together exceed
 * the maximum frame size L2Framesize, l2_sendto fails.
 */

// L2SAP: struct for sockets
    // int: socket, struct sockaddr_in 

// sockaddr_in - tar inn AF_INET, portnummer, IPv4-adresse

// sendto tar inn: socket file descriptor fra client,
// et buffer som skal skrives til (størrelse L2Payloadsize)



int l2sap_sendto( L2SAP* client, const uint8_t* data, int len )
{

    // Hvis datamengden er for stor
    if (len + L2Headersize > L2Framesize) {
        perror("Data exceeds frame size");
        return -1;
    }

    int socketFD = client->socket;
    struct sockaddr_in reciever = client->peer_addr;

    // Kaller på hjelpefunksjon for å opprette header
    struct L2Header* header = malloc(L2Headersize);
    if (header == NULL) {
        perror("Failed to allocate memory");
        return -1;
    } 

    header->dst_addr = reciever.sin_addr.s_addr;
    header->len = (uint16_t)len;

    // Setter foreløpig checksum til 0
    header->checksum = 0;
    header->mbz = 0;

    // Oppretter buffer (for hele rammen)
    int bufsize = L2Headersize + len;
    uint8_t* buffer = malloc(bufsize);
    if (buffer == NULL) {
        free(header);
        perror("Error mallocing space for buffer");
        return -1;
    }

    // Legger header og payload inn i buffer
    memcpy(buffer, header, L2Headersize);
    memcpy(buffer+L2Headersize, data, len);

    // Kaller på hjelpefunksjon for å sette checksum
    uint8_t cs = compute_checksum(buffer, bufsize);
    header->checksum = cs;

    memcpy(buffer, header, L2Headersize);


    // Sender melding
    sendto(socketFD, data, len, 0, (const struct sockaddr*)&reciever, sizeof(reciever));
    return 1;
}



/* Convenience function. Calls l2sap_recvfrom_timeout with NULL timeout
 * to make it waits endlessly.
 */
int l2sap_recvfrom( L2SAP* client, uint8_t* data, int len )
{
    return l2sap_recvfrom_timeout( client, data, len, NULL );
}

/* l2sap_recvfrom_timeout waits for data from a remote UDP sender, but
 * waits at most timeout seconds.
 * It is possible to pass NULL as timeout, in which case
 * the function waits forever.
 *
 * If a frame arrives in the meantime, it stores the remote
 * peer's address in peer_address and its size in peer_addr_sz.
 * After removing the header, the data of the frame is stored
 * in data, up to len bytes.
 *
 * If data is received, it returns the number of bytes.
 * If no data is reveid before the timeout, it returns L2_TIMEOUT,
 * which has the value 0.
 * It returns -1 in case of error.
 */
int l2sap_recvfrom_timeout( L2SAP* client, uint8_t* data, int len, struct timeval* timeout )
{
    fprintf( stderr, "%s has not been implemented yet\n", __FUNCTION__ );
    return -1;
}

