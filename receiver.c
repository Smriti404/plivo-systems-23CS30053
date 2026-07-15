#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#define RELAY_PORT 47002
#define PLAYER_PORT 47020
#define FRAME_SIZE 164

// Buffers for storing incoming frames
char playout_buffer[8192][FRAME_SIZE];
int ready[8192];
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *play(void *a) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(PLAYER_PORT)};
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    uint32_t n = 0;
    while(1) {
        usleep(20000); // 20ms playout interval
        pthread_mutex_lock(&m);
        if (ready[n % 8192]) {
            sendto(s, playout_buffer[n % 8192], FRAME_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
            ready[n % 8192] = 0;
        }
        pthread_mutex_unlock(&m);
        n++;
    }
}

int main() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    // Use environment variable if it exists, otherwise default to 47002
    char *port_env = getenv("RECEIVER_PORT");
    if (!port_env) port_env = getenv("PORT");
    addr.sin_port = htons(port_env ? atoi(port_env) : RELAY_PORT);
    
    addr.sin_addr.s_addr = INADDR_ANY; 

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        exit(1);
    }
    
    
    printf("[Receiver] Bound to port %d and listening...\n", RELAY_PORT);

    pthread_t t; 
    pthread_create(&t, NULL, play, NULL);
    
    char rb[328];
    while(1) {
        int l = recv(s, rb, 328, 0);
        if (l > 0) {
            printf("Received %d bytes!\n", l);
            pthread_mutex_lock(&m);
            for(int i = 0; i < l / FRAME_SIZE; i++) {
                uint32_t seq;
                memcpy(&seq, rb + (i * FRAME_SIZE), 4);
                int idx = ntohl(seq) % 8192;
                memcpy(playout_buffer[idx], rb + (i * FRAME_SIZE), FRAME_SIZE);
                ready[idx] = 1;
            }
            pthread_mutex_unlock(&m);
        }
    }
}