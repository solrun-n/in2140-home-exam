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
static uint8_t compute_checksum( const uint8_t* frame, int len );



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
        free(l2sap);
        return NULL;
    }

    // Initialiserer struct sockaddr_in som skal inn i l2sap
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = inet_pton(AF_INET, server_ip, &addr);

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

// sendto skal bruke en socket for å 

int l2sap_sendto( L2SAP* client, const uint8_t* data, int len )
{

    struct sockaddr_in reciever = client->peer_addr;

    inet_pton(AF_INET, reciever.sin_addr, &reciever);




    fprintf( stderr, "%s has not been implemented yet\n", __FUNCTION__ );
    return -1;
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

