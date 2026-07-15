#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define RELAY_PORT 47001

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(RELAY_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char buffer[164]; 
    uint32_t seq = 0;

    while (seq < 1000) { // Send 1000 packets and exit
        memset(buffer, 0, 164);
        
        // Fill with fake data instead of reading from stdin
        memset(buffer + 4, 'A', 160); 
        
        uint32_t net_seq = htonl(seq);
        memcpy(buffer, &net_seq, 4);

        sendto(sock, buffer, 164, 0, (struct sockaddr *)&addr, sizeof(addr));
        
        seq++;
        usleep(20000); 
    }
    close(sock);
    printf("Sender finished successfully.\n");
    return 0;
}