==== README ====

# Lag 2



# Lag 4

## Initiell implementasjon
I vår første implementasjon av L4 tok vi ikke stilling til innkommende data-pakker i l4sap_send. 
For å implementere full-duplex startet vi med å kunne motta datapakker i send-funksjonen. 



En videre løsning vi lagde kunne håndtere et pakketap på t.o.m. 4 %. Problemet vi møtte på ved høyere
packet loss, var at klienten "brukte opp" de fire forsøkene den hadde på å vente på acks, på å heller
motta data. Ved å logge timestamps for når vi sendte pakker, fant vi ut av at retransmitting av pakker
skjedde utrolig raskt, på samme milisekund. 


I løsningen vi har levert, retransmitter vi kun dersom vi mottar en feil ack eller får en timeout, altså
at det ikke kom noen data via L2. Med andre ord blir ikke forsøk brukt opp på å motta data. Dersom en 
datapakke mottas i send samtidig som vi venter på en ack, sendes en ack for denne pakken, og dataen
lagres på en buffer. Når recieve kalles leses det først fra bufferet, om det er noe data der.
Vi kom frem til denne løsningen fordi den ligner på oppførselen vi ser fra serveren - vi sender en ack
og lagrer pakken, men håndterer den ikke før send har returnert (altså fått en korrekt ack).

Oppgaveteksten er litt utydelig, men vi mener vi har kommet frem til en løsning som er innenfor rammene
av oppgaven. 

Oppgaven sier at:
- Funksjonen skal ikke returnere før den har fått en riktig ack for avsendt pakke - med mindre
pakken er forsøkt sendt fire ganger

Vi øker antall forsøk (attempts) kun når vi får en timeout mens vi venter på acken. 
Som oppgaveteksten sier: "Waiting for a correct ACK may fail after a timeout of 1 second
The function retransmits the packet in that case. The function attempts up to 4 retransmissions."
Vi tolker dette som at vi kun skal sende pakken på nytt, dersom vi får en timeout.
Vi har derfor brukt en while-løkke som kontinuerlig mottar pakker frem til den enten får en timeout
eller får riktig ack. Vår nåværende implementasjon klarer opp til 15 % packet loss. 





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


