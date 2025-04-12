#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "l4sap.h"
#include "l2sap.h"

/* Create an L4 client.
 * It returns a dynamically allocated struct L4SAP that contains the
 * data of this L4 entity (including the pointer to the L2 entity
 * used).
 */
L4SAP* l4sap_create( const char* server_ip, int server_port )
{

    // Må allokere minne for L4SAP
    L4SAP* l4sap = malloc(sizeof(struct L4SAP));
    if (l4sap == NULL) {
        perror("Error mallocing L4SAP");
        exit(EXIT_FAILURE);
    }

    // Oppretter en L2-klient som legges inn i L4-klienten
    L2SAP* l2 = l2sap_create(server_ip, server_port);
    l4sap->l2sap = l2;
    l4sap->current_ack = 0;
    l4sap->current_seq = 0;

    // Timeout osv 

    return l4sap;
}

/* The functions sends a packet to the network. The packet's payload
 * is copied from the buffer that it is passed as an argument from
 * the caller at L5.
 * If the length of that buffer, which is indicated by len, is larger
 * than L4Payloadsize, the function truncates the message to L4Payloadsize.
 *
 * The function does not return until the correct ACK from the peer entity
 * has been received.
 * When a suitable ACK arrives, the function returns the number of bytes
 * that were accepted for sending (the potentially truncated packet length).
 *
 * Waiting for a correct ACK may fail after a timeout of 1 second
 * (timeval.tv_sec = 1, timeval.tv_usec = 0). The function retransmits
 * the packet in that case.
 * The function attempts up to 4 retransmissions. If the last retransmission
 * fails with a timeout as well, the function returns L4_SEND_FAILED.
 *
 * The function may also return:
 * - L4_QUIT if the peer entity has sent an L4_RESET packet.
 * - another value < 0 if an error occurred.
 */



 // Hjelpefunksjon for å oppdatere seq/ack
 uint8_t update(uint8_t bit) {
    return if (bit == 0) 1 else 0;
 }



int l4sap_send( L4SAP* l4, const uint8_t* data, int len )
{

    // Sjekker at datamengden ikke er for stor
    // Hvis datamengden er for stor
    if (len > L4Payloadsize) {

        // må kutte ned på pakken
    }

    // data: kun payload som skal sendes

    // Hvis seq og ack-nr vi er på nå er like, øker vi seq
    if (l4->current_seq == l4->current_ack) {
        l4->current_seq = update(current_seq);
    }

    uint8_t seq = l4->current_seq;

    // Oppretter L4-header
    



    struct L2SAP* l2sap = l4->l2sap;



    // For å kunne sende en pakke:
    // 






    // Første gang: seq er 0
    // om vi mottar ack 0, kan vi øke seq til 1
    // om vi mottar en ack 1 mens seq er 0 vet vi at noe har gått GÆLI

    // ved korrekt ack: oppdaterer seq og returnerer


    fprintf( stderr, "%s has not been implemented yet\n", __FUNCTION__ );
    return L4_QUIT;
}

/* The functions receives a packet from the network. The packet's
 * payload is copy into the buffer that it is passed as an argument
 * from the caller at L5.
 * The function blocks endlessly, meaning that experiencing a timeout
 * does not terminate this function.
 * The function returns the number of bytes copied into the buffer
 * (only the payload of the L4 packet).
 * The function may also return:
 * - L4_QUIT if the peer entity has sent an L4_RESET packet.
 * - another value < 0 if an error occurred.
 */
int l4sap_recv( L4SAP* l4, uint8_t* data, int len )
{

    // Her må ack og seq være ulike eller noe sånt
    // Fra headeren
    uint8_t type = data[0];
    uint8_t seq = data[1]
    uint8_t ack = data[2];



    fprintf( stderr, "%s has not been implemented yet\n", __FUNCTION__ );
    return L4_QUIT;
}

/** This function is called to terminate the L4 entity and
 *  free all of its resources.
 *  We recommend that you send several L4_RESET packets from
 *  this function to ensure that the peer entity is also
 *  terminating correctly.
 */
void l4sap_destroy( L4SAP* l4 )
{
    fprintf( stderr, "%s has not been implemented yet\n", __FUNCTION__ );
}

