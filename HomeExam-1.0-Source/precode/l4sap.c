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

    printf("----L4 SEND CALLED----\n");
    
    // Kutter pakken om datamengden er for stor
    // Her oppdaterer vi kun lengden, fordi memcpy brukes senere
    // for å faktisk sende pakken, og da definerer vi lengden med len
    if (len > L4Payloadsize) {
        printf("Kutter datapakke fra %d til %d bytes\n", len, L4Payloadsize);
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
    int packetsize = L4Headersize + len;
    uint8_t* packet = malloc(packetsize);
    if (packet == NULL) {
        free(header);
        perror("Error mallocing space for buffer");
        return -1;
    }
    printf("Total pakkestørrelse: %d bytes\n", packetsize);

    memcpy(packet, header, L4Headersize); // Legger først inn header
    memcpy(packet+L4Headersize, data, len); // Så datapakken 
    // her er len oppdatert dersom pakken var for stor, så den overskrider ikke strl

    // Forsøker avsending av pakke maks 4 ganger
    int result = L4_SEND_FAILED;
    
    for (int attempt = 1; attempt <= 4; attempt++) {

        // Sender pakken via L2
        printf("Forsøk: %d. Sender %d bytes\n", attempt, len);
        int send = l2sap_sendto(l4->l2sap, packet, len+L4Headersize);
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
        printf("Antall bytes mottatt fra l2_recieve_timeout: %d bytes\n", recieved);

        // En pakke har ankommet
        if (recieved > 0) {

            // Henter ut header
            struct L4Header* recv_header = (struct L4Header*)buffer;
            printf("Header mottatt fra L2-pakke:\n");
            printf("  Type:   %d\n", recv_header->type);
            printf("  Seqno:  %d\n", recv_header->seqno);
            printf("  Ackno:  %d\n", recv_header->ackno);
            printf("  MBZ:    %d\n", recv_header->mbz);

            // Her kan mottatt pakke være ACK eller RESET
            if (recv_header->type == L4_ACK) {
                printf("Pakke er ACK\n");
                printf("Ack: %d, current seq: %d\n", recv_header->ackno, l4->current_seq);


                // Serveren sender ack 1 for pakke 0. Vi gjør en xor på current_seq for
                // å finne forventet ack, fordi serveren sender seq for "neste pakke"
                // som en ack for å indikere at den er klar for en ny pakke
                if (recv_header->ackno == (l4->current_seq ^ 1)) { 
                printf("ACK stemmer, oppdaterer seq fra %d til %d\n", l4->current_seq, (l4->current_seq ^ 1));
                    l4->current_seq ^= 1; // Oppdaterer seq med XOR (flipper 0/1)
                    result = len;
                    break;
                }

            } else if (recv_header->type == L4_RESET) { // Resetter
                printf("Pakke er reset. Avbryter\n");
                l4->reset = 1;
                result = L4_QUIT;
                break;
            } else if (recv_header->type == L4_DATA) {
                printf("Mottatt data-pakke i send. Forkaster pakken\n");
                continue;
            }
        } // Ingen pakke, prøv på nytt
    }
    printf("Loopen er over, frigjør alt minne\n");

    free(header); // Frigjøre minne
    free(packet);
    printf("----L4 END----\n");
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




 // Kaller på recieve i L2 for å vente på en data-pakke fra L5
 // Fra L2 mottar den en L4header + payload
 // headeren må sjekkes, payload skal legges inn i data og lengden skal returneres
 // sender ack via l2sendto

 // Ansvaret til denne funksjonen er å motta datapakker
int l4sap_recv( L4SAP* l4, uint8_t* data, int len )
{
    printf("----L4 RECIEVE CALLED----\n");
    printf("Antall bytes på parameter: %d bytes\n", len);

    uint8_t buffer[L2Framesize];
    int recieved = l2sap_recvfrom(l4->l2sap, buffer, sizeof(buffer));
    printf("Mottatt %d bytes fra L2\n", recieved);
    if (recieved < 0) {
        perror("Error recieving frame from L2");
        return -1;
    }

    // Henter ut headeren
    struct L4Header* recv_header = (struct L4Header*)buffer;

    // Hvis en reset-pakke blir sendt 
    if (recv_header->type == L4_RESET) {
        printf("Mottatt reset-pakke\n");
        l4->reset = 1;
        return L4_QUIT;
    } else if (recv_header->type == L4_DATA) {
        printf("Mottatt data-pakke\n");

        // Fjerne headeren 
        // Oppdaterer pointer til å peke på data etter header
        uint8_t* payload = buffer + L4Headersize;
        int payload_size = recieved - L4Headersize;
        printf("Fjernet gammel L4-header. Payload size: %d bytes\n", payload_size);
        memcpy(data, payload, payload_size);

        // opprett ny header med ack
        struct L4Header ack;
        ack.type = L4_ACK;
        ack.seqno = 0; // ikke viktig for ACK?
        ack.ackno = (recv_header->seqno ^ 1); // mottatt pakke
        ack.mbz = 0;

        // Send ack
        if (l2sap_sendto(l4->l2sap, (uint8_t*)&ack, L4Headersize) != 1) {
            perror("Failed to send ACK");
            return -1;
        }

        return payload_size;
    }

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

