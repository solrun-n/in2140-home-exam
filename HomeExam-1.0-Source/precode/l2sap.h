#ifndef L2SAP_H
#define L2SAP_H

#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define L2Framesize   1024
#define L2Headersize  (int)(sizeof(struct L2Header))
#define L2Payloadsize (int)(L2Framesize-L2Headersize)

#define L2_TIMEOUT    0

typedef struct L2Header L2Header;

struct L2Header {
    uint32_t dst_addr;
    uint16_t len;
    uint8_t  checksum;
    uint8_t  mbz;
};

typedef struct L2SAP L2SAP;

struct L2SAP {
    int                socket;
    struct sockaddr_in peer_addr;
};

struct L2SAP* l2sap_server_create( int port );
struct L2Header l2sap_addheader(struct sockaddr_in addr, int len) ;

L2SAP* l2sap_create( const char* server_ip, int server_port );
void l2sap_destroy( L2SAP* client );
int  l2sap_sendto( L2SAP* client, const uint8_t* data, int len );
int  l2sap_recvfrom_timeout( L2SAP* client, uint8_t* data, int len, struct timeval* timeout );
int  l2sap_recvfrom( L2SAP* client, uint8_t* data, int len );


#endif

