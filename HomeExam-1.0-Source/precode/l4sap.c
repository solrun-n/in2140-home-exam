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

    // Fyller ut feltene
    l4sap->current_seq = 0;
    l4sap->reset = 0; 
    
    l4sap->timeout.tv_sec = 1;
    l4sap->timeout.tv_usec = 0;

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


int l4sap_send( L4SAP* l4, const uint8_t* data, int len )
{
    
    // Kutter pakken om datamengden er for stor
    // Her oppdaterer vi kun lengden, fordi memcpy brukes senere
    // for å faktisk sende pakken, og da definerer vi lengden med len
    if (len > L4Payloadsize) {
        len = L4Payloadsize;
    }

    // Oppretter en L4 header
    // Sjekk om vi kan unngå å allokere header
    struct L4Header* header = malloc(L4Headersize);
    if (header == NULL) {
        perror("Failed to allocate memory");
        return -1;
    } 

    // Fyller inn headeren

    // Klienten sender alltid data, med mindre serveren har sendt en reset
    if (l4->reset) {
        header->type = L4_QUIT;
    } else header->type = L4_DATA;

    header->seqno = l4->current_seq; // Nåværende sekvensnr legges inn 
    header->ackno = l4->current_seq; // Forventet ack = seq
    header->mbz = 0;

    // Legger headeren på en buffer med datapakken
    uint8_t* packet = malloc(L4Headersize + len);
    if (packet == NULL) {
        free(header);
        perror("Error mallocing space for buffer");
        return -1;
    }
    memcpy(packet, header, L4Headersize); // Legger først inn header
    memcpy(packet+L4Headersize, data, len); // Så datapakken 
    // her er len oppdatert dersom pakken var for stor, så den overskrider ikke strl

    
    // Forsøker avsending av pakke maks 4 ganger
    int result = L4_SEND_FAILED;
    uint8_t attempt = 1;

    while (attempt <= 4) {

        // Sender pakken via L2
        int send = l2sap_sendto(l4->l2sap, packet, len);
        if (send != 1) {
            perror("Error sending frame from L2");
            free(header);
            free(packet);
            return -1;
        }

        // Resetter timeout hver runde 
        l4->timeout.tv_sec = 1;
        l4->timeout.tv_usec = 0;


        // Oppretter et buffer der mottatt data skal legges
        // og kaller på recieve i L2 for å hente ut ack
        // L2 legger L4-pakken på bufferet
        uint8_t buffer[L2Framesize]; 
        int recieved = l2sap_recvfrom_timeout(l4->l2sap, buffer, sizeof(buffer), &l4->timeout);

        // En pakke har ankommet
        if (recieved > 0) {

            // Henter ut header
            struct L4Header* recv_header = (struct L4Header*)buffer;

            // Her kan mottatt pakke være ACK eller RESET
            if (recv_header->type == L4_ACK) {

                if (recv_header->ackno == l4->current_seq) {
                    l4->current_seq ^= 1; // Oppdaterer seq med XOR (flipper 0/1)
                    result = len;
                    break;
                }

            } else if (recv_header->type == L4_RESET) { // Resetter
                l4->reset = 1;
                result = L4_QUIT;
                break;
            }       
        } else attempt++; // Ingen pakke, prøv på nytt
    }

    free(header); // Frigjøre minne
    free(packet);
    return result;
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


 // Ansvaret til denne funksjonen er å motta datapakker
int l4sap_recv( L4SAP* l4, uint8_t* data, int len )
{

    // Henter ut headeren
    struct L4Header* recv_header = (struct L4Header*)data;

    // Hvis en reset-pakke blir sendt 
    if (recv_header->type == L4_RESET) {
        l4->reset = 1;
        return L4_QUIT;
    } 

    // Ellers skal klienten bare motta ack?



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
    // Sender L4_RESET 3 ganger (bør kanskje være flere?)
    for (int i = 0; i < 3; i++) {
        // Må sende reset her, ikke data (som sendto vanligvis gjør)
    }

    l2sap_destroy(l4->l2sap);
    free(l4);
}

