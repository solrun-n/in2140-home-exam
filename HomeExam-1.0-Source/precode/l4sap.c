#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>       // for struct tm, localtime, strftime


#include "l4sap.h"
#include "l2sap.h"


L4SAP* l4sap_create( const char* server_ip, int server_port )
{

    // Må allokere minne for L4SAP
    L4SAP* l4sap = malloc(sizeof(struct L4SAP));
    if (l4sap == NULL) {
        printf("Error mallocing L4SAP\n");
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


int l4sap_send( L4SAP* l4, const uint8_t* data, int len )
{
    
    // Kutter pakken om datamengden er for stor
    // Her oppdaterer vi kun lengden, fordi memcpy brukes senere
    // for å faktisk sende pakken, og da definerer vi lengden med len
    if (len > L4Payloadsize) {
        len = L4Payloadsize;
    }

    // Allokerer headeren på stack
    struct L4Header header;

    // Klienten sender alltid data, med mindre serveren har sendt en reset
    if (l4->reset) {
        header.type = L4_QUIT;
    } else header.type = L4_DATA;

    header.seqno = l4->current_seq_send; // Nåværende sekvensnr legges inn 
    header.ackno = 0; // ack er ikke relevant her, settes alltid til 0
    header.mbz = 0;

    // Legger headeren på en buffer med datapakken
    int packetsize = L4Headersize + len;
    uint8_t* packet = malloc(packetsize);
    if (packet == NULL) {
        perror("Error mallocing space for buffer");
        return -1;
    }

    memcpy(packet, &header, L4Headersize); // Legger først inn header
    memcpy(packet+L4Headersize, data, len); // Så datapakken 
    // Her er len oppdatert dersom pakken var for stor, så den overskrider ikke strl

    int result = L4_SEND_FAILED;
    
    // Forsøker avsending av pakke maks 4 ganger
    for (int attempt = 1; attempt <= 4; attempt++) {

        int send = l2sap_sendto(l4->l2sap, packet, len+L4Headersize);
        if (send != 1) {
            perror("Error sending frame from L2");
            continue;
        }  
        
        // Resetter timeout hver runde 
        l4->timeout.tv_sec = 1;
        l4->timeout.tv_usec = 0;

        uint8_t buffer[L2Framesize]; 
        int received = 0; // Boolean for mottatt data
        int is_ack_received = 0; // Boolean for mottatt ack

        // Mottar data fortløpende så lenge vi ikke har timeout
        while(1) {
            received = l2sap_recvfrom_timeout(l4->l2sap, buffer, sizeof(buffer), &l4->timeout);
            if (received <= 0) {
                break; // Timeout hvis vi ikke mottar data fra L2
            }

            struct L4Header* recv_header = (struct L4Header*)buffer;

            // Sjekker om vi har mottatt ack og den er riktig
            if (recv_header->type == L4_ACK && recv_header->ackno == (l4->current_seq_send ^ 1)) {
                is_ack_received = 1; // Ack ok
                l4->current_seq_send ^= 1; // Oppdater neste seq som skal sendes
                result = len; // Oppdater returverdi
                break; 

            // Om vi mottar feil ack, data eller reset
            } else {
                if (recv_header->type == L4_RESET) {
                    l4->reset = 1;
                    l4sap_destroy(l4);
                    return L4_QUIT;
        
                } else if (recv_header->type = L4_DATA) {
                
                    int sent_ack = send_ack(l4, recv_header); 
                    if (sent_ack < 0) {
                        perror("Error sending ack");
                        continue;
                    } 
                    l4->last_ack_sent = sent_ack;

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
                        continue;
                    }
                }
        
            }
        }

        // If ACK was not received, increment attempt and retry
        if (!is_ack_received) {
            printf("SEND: Ingen ACK, prøver på nytt...\n");
        } else {
            printf("SEND: ACK mottatt, avslutter sending...\n");
            break; // Exit attempts, as we received ACK    
        }
    }

    // Frigjør minne
    free(packet);

    return result; // Return result of sending
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
int l4sap_recv( L4SAP* l4, uint8_t* data, int len ) {

    // Returnerer fra bufferet om det ligger noe data der
    if (l4->has_pending_data) {
        memcpy(data, l4->pending_data, l4->pending_len);
        l4->has_pending_data = 0;
        return l4->pending_len;
    }

    // Loopen går evig til det kommer en ny data-pakke
    uint8_t buffer[L2Framesize];

    while(1) {

        int received = l2sap_recvfrom(l4->l2sap, buffer, sizeof(buffer));
        if (received < 0) {
            printf("Error recieving frame from L2\n");
            continue;
        }

        // Henter ut headeren
        struct L4Header* recv_header = (struct L4Header*)buffer;

        // Hvis en reset-pakke blir sendt 
        if (recv_header->type == L4_RESET) {
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
 void l4sap_destroy(L4SAP* l4)
 {
    
     // Oppretter headeren 
     struct L4Header reset_header;
     reset_header.type = L4_RESET;
     reset_header.seqno = 0;
     reset_header.ackno = 0;
     reset_header.mbz = 0;
 
     // Sender RESET mange ganger
     for (int i = 0; i < 10; i++) {
         int sendReset = l2sap_sendto(l4->l2sap, (uint8_t*)&reset_header, L4Headersize);
         if (sendReset != 1) {
             perror("Error sending reset message\n");
         } else {
             printf("Sent reset message to peer entity\n");
         }
     }
 
     // Frigjør minnet
     l2sap_destroy(l4->l2sap);
     free(l4);
 }