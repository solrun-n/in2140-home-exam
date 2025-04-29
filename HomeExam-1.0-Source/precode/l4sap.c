#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


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
    l4sap->current_seq_send = 0; 
    l4sap->last_seq_received = 1; // første mottatte seq vil alltid være 0
    l4sap->last_ack_sent = 0;
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

    header->seqno = l4->current_seq_send; // Nåværende sekvensnr legges inn 
    header->ackno = 0; // ack er ikke relevant her, settes alltid til 0
    header->mbz = 0;

    // Legger headeren på en buffer med datapakken
    int packetsize = L4Headersize + len;
    uint8_t* packet = malloc(packetsize);
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
    
    for (int attempt = 1; attempt <= 4; attempt++) {

        // Sender pakken via L2
        // Sender kun dersom vi er ferdige med forrige pakke
        printf("SEND: Current seq før avsending av pakke: %d\n", l4->current_seq_send);
        
        int send = l2sap_sendto(l4->l2sap, packet, len+L4Headersize);
        printf("SEND: Sendte pakke på forsøk %d: seq: %d\n", attempt, header->seqno);
        printf("SEND: Pakkens størrelse: %d\n", len);
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
        // og kaller på receive i L2 for å hente ut ack
        // L2 legger L4-pakken på bufferet
        uint8_t buffer[L2Framesize]; 
        int received = l2sap_recvfrom_timeout(l4->l2sap, buffer, sizeof(buffer), &l4->timeout);

        // En pakke har ankommet
        if (received > 0) {

            // Henter ut header
            struct L4Header* recv_header = (struct L4Header*)buffer;

            // Mottar pakke for å sende ack
            if (recv_header->type == L4_ACK) {
                printf("SEND: mottok ack-pakke: ack: %d\n", recv_header->ackno);

                // Serveren sender ack 1 for pakke 0. Vi gjør en xor på current_seq_send for
                // å finne forventet ack, fordi serveren sender seq for "neste pakke"
                // som en ack for å indikere at den er klar for en ny pakke
                if (recv_header->ackno == (l4->current_seq_send ^ 1)) { 
                    printf("SEND: ack ok: oppdaterer current_seq_send til %d\n", l4->current_seq_send^1);
                    l4->current_seq_send ^= 1; // Oppdaterer seq med XOR (flipper 0/1)
                    result = len;
                    break;
                } printf("SEND: feil ack mottatt\n");

            } else if (recv_header->type == L4_RESET) { // Resetter
                printf("MOTTOK RESET-pakke\n");
                l4->reset = 1;
                result = L4_QUIT;
                break;

            } else if (recv_header->type == L4_DATA) {
                printf("MOTTOK DATA-pakke i sendto. Seq = %d\n", recv_header->seqno);

                int sent_ack = send_ack(l4, recv_header); // Kaller på hjelpefunksjon
                if (sent_ack < 0) {
                    perror("Error sending ack");
                    free(header);
                    free(packet);
                    return -1;
                } 

                // Sjekker om mottatt pakke er duplikat
                // Hvis duplikat: ignorer, hvis ny pakke: legg i buffer
                if (recv_header->seqno == l4->last_seq_received) { // Duplikat
                    printf("SEND: mottok duplikat data-pakke, går videre\n");
                    continue;
                } 

                // Ny pakke: legger på buffer og oppdaterer last seq recv
                printf("SEND: mottok ny data-pakke. Legger på buffer\n");
                if (!l4->has_pending_data) {
                    l4->last_seq_received = recv_header->seqno;
                    memcpy(l4->pending_data, buffer + L4Headersize, received - L4Headersize);
                    l4->pending_len = received - L4Headersize;
                    l4->has_pending_data = 1;
                }
  
            }
        } 
        printf("SEND: timeout for å motta ack. Prøver igjen\n");
        // Ingen pakke, prøv på nytt
    }

    printf("SEND: frigjør minne og returnerer\n");

    free(header); // Frigjøre minne
    free(packet);
    return result;
}


// Hjelpefunksjon for å sende en ack
// Den returnerer acken den har sendt
int send_ack(L4SAP* l4, struct L4Header* recv_header) {

    // Oppretter en L4-header
    struct L4Header ack_header;
    ack_header.type = L4_ACK;
    ack_header.seqno = 0; // Har ingenting å si for ack
    ack_header.mbz = 0;

    // ACK vi skal sende avhenger av om pakken vi har fått inn 
    // er en helt ny pakke, eller en resendt pakke (hvis ack har blitt 
    // ødelagt på vei til server)

    // Hvis seq på den innkommende pakken ikke er det samme
    // som seq til forrige avsendte pakke, kan vi sende ny ack
    if (recv_header->seqno != l4->last_seq_received) {
        ack_header.ackno = (recv_header->seqno ^ 1);

    // Hvis seq på innkommende pakke = seq på forrige avsendte pakke,
    // betyr det at samme pakken har kommet på nytt. Vi sender da "gammel" ack
    } else {
        ack_header.ackno = l4->last_ack_sent; 
    }

    // Sender headeren via L2
    int send = l2sap_sendto(l4->l2sap, (uint8_t*)&ack_header, L4Headersize);
    if (send != 1) {
        printf("SEND ACK: feil ved avsending av ack\n");
        perror("Error sending ack from L2");
        return -1;
    }

    printf("SEND ACK: Sendte ACK fra klient til server: seq = %d, ack = %d\n", recv_header->seqno, ack_header.ackno);
    //l4->last_ack_sent = ack_header.ackno;
    return ack_header.ackno;
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


 // Kaller på receive i L2 for å vente på en data-pakke fra L5
 // Fra L2 mottar den en L4header + payload
 // headeren må sjekkes, payload skal legges inn i data og lengden skal returneres
 // sender ack via l2sendto


 // Ansvaret til denne funksjonen er å motta datapakker og sende acks
int l4sap_recv( L4SAP* l4, uint8_t* data, int len )
{

    if (l4->has_pending_data) {
        memcpy(data, l4->pending_data, l4->pending_len);
        l4->has_pending_data = 0;
        return l4->pending_len;
    }

    // Loopen går evig til det kommer en ny data-pakke
    // Da må denne håndteres
    uint8_t buffer[L2Framesize];
    int counter = 0;

    while(1) {
        printf("RECV: While loop runde %d\n", counter);
        counter++;

        int received = l2sap_recvfrom(l4->l2sap, buffer, sizeof(buffer));
        if (received < 0) {
            perror("Error recieving frame from L2");
            continue;
        }

        // Henter ut headeren
        struct L4Header* recv_header = (struct L4Header*)buffer;

        // Hvis en reset-pakke blir sendt 
        if (recv_header->type == L4_RESET) {
            printf("RECV: Mottok reset-pakke\n");
            l4->reset = 1;
            return L4_QUIT;

        
        } else if (recv_header->type == L4_ACK) {
            printf("-------------------RECV: mottok ack\n");
            


            // Hvis datapakke: send ack og sjekk om duplikat, hvis duplikat fortsett å vent på ny pakke
        } else if (recv_header->type == L4_DATA) {

            printf("RECV: Mottatt DATA-pakke fra server med seq = %d\n", recv_header->seqno);

            // Sender ack via hjelpefunksjon
            int sent_ack = send_ack(l4, recv_header); 
            if (sent_ack < 0) {
                perror("Error sending ack");
                return -1;
            } 
            l4->last_ack_sent = sent_ack;

            // Hvis duplikat (samme seq som forrige pakke den mottok)
            if (recv_header->seqno == l4->last_seq_received) {
                printf("RECV: Duplikat!\n");
                continue; // Går tilbake til start på while-løkken
            }
            

            // Hvis det er en duplikatpakke skal loopen fortsette å gå
            // og vi skal ikke oppdatere last_seq_received eller last_ack_sent


            // Ny pakke: 
            // Oppdaterer last ack til acken vi akkurat sendte 
            l4->last_seq_received = recv_header->seqno;

            printf("RECV: last_ack_sent = %d\n", l4->last_ack_sent);
            printf("RECV: last seq received = %d\n", l4->last_seq_received);


            // Fjerner header og returnerer payload_size
            // Oppdaterer pointer til å peke på data etter header
            uint8_t* payload = buffer + L4Headersize;
            int payload_size = received - L4Headersize;
            memcpy(data, payload, payload_size);

            return payload_size;
        }
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