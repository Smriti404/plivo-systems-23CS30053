#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define RELAY_PORT 47002
#define PLAYER_PORT 47020
#define FRAME_SIZE 164
#define HISTORY_SIZE 2
#define BUFFER_SIZE 8192

typedef struct {
    uint32_t seq;
    char payload[160];
} __attribute__((packed)) Frame;

Frame playout_buffer[BUFFER_SIZE];
int frame_received[BUFFER_SIZE];
pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;

double t0 = 0.0;
int delay_ms = 0;

void *playout_thread(void *arg) {
    int sock_out = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr_out;
    memset(&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    addr_out.sin_port = htons(PLAYER_PORT);
    addr_out.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint32_t next_seq = 0;
    double start_time_ms = (t0 * 1000.0) + delay_ms;

    while (1) {
        double deadline_ms = start_time_ms + (next_seq * 20.0);
        
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        double now_ms = (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);

        double sleep_ms = deadline_ms - now_ms;
        if (sleep_ms > 0) {
            struct timespec req;
            req.tv_sec = (time_t)(sleep_ms / 1000.0);
            req.tv_nsec = (long)((sleep_ms - (req.tv_sec * 1000.0)) * 1000000.0);
            nanosleep(&req, NULL);
        }

        int idx = next_seq % BUFFER_SIZE;
        pthread_mutex_lock(&buf_mutex);
        if (frame_received[idx]) {
            uint32_t buf_seq = ntohl(playout_buffer[idx].seq); // Convert network to host byte order!
            if (buf_seq == next_seq) {
                sendto(sock_out, &playout_buffer[idx], FRAME_SIZE, 0,
                       (struct sockaddr *)&addr_out, sizeof(addr_out));
            }
        }
        pthread_mutex_unlock(&buf_mutex);

        next_seq++;
    }

    close(sock_out);
    return NULL;
}

int main() {
    char *t0_env = getenv("T0");
    char *delay_env = getenv("DELAY_MS");
    if (t0_env) t0 = strtod(t0_env, NULL);
    if (delay_env) delay_ms = atoi(delay_env);

    int sock_in = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr_in;
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(RELAY_PORT);
    addr_in.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(sock_in, (struct sockaddr *)&addr_in, sizeof(addr_in)) < 0) {
        perror("bind");
        return 1;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, playout_thread, NULL);

    char recv_buf[HISTORY_SIZE * FRAME_SIZE];
    while (1) {
        ssize_t len = recv(sock_in, recv_buf, sizeof(recv_buf), 0);
        if (len < FRAME_SIZE) continue;

    // Determine frame count implicitly using the UDP payload length
    int count = len / FRAME_SIZE;
    if (count > HISTORY_SIZE) count = HISTORY_SIZE;

    pthread_mutex_lock(&buf_mutex);
    for (int i = 0; i < count; i++) {
        Frame f;
        memcpy(&f, &recv_buf[i * FRAME_SIZE], FRAME_SIZE);
        uint32_t seq = ntohl(f.seq); 

        int idx = seq % BUFFER_SIZE;
        if (!frame_received[idx] || seq > ntohl(playout_buffer[idx].seq)) {
            playout_buffer[idx] = f;
            frame_received[idx] = 1;
        }
    }
    pthread_mutex_unlock(&buf_mutex);
}
    close(sock_in);
    return 0;
}