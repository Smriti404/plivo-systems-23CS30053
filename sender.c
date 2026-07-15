#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SRC_PORT 47010
#define RELAY_PORT 47001
#define FRAME_SIZE 164
#define HISTORY_SIZE 2

typedef struct {
    uint32_t seq;
    char payload[160];
} __attribute__((packed)) Frame;

int main() {
    int sock_in = socket(AF_INET, SOCK_DGRAM, 0);
    int sock_out = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr_in, addr_out;
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(SRC_PORT);
    addr_in.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(sock_in, (struct sockaddr *)&addr_in, sizeof(addr_in)) < 0) {
        perror("bind");
        return 1;
    }

    memset(&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    addr_out.sin_port = htons(RELAY_PORT);
    addr_out.sin_addr.s_addr = inet_addr("127.0.0.1");

    Frame history[HISTORY_SIZE];
    memset(history, 0, sizeof(history));
    int history_count = 0;

    char recv_buf[FRAME_SIZE];
    while (1) {
        ssize_t len = recv(sock_in, recv_buf, FRAME_SIZE, 0);
        if (len < FRAME_SIZE) continue;

        Frame current;
        memcpy(&current, recv_buf, FRAME_SIZE);

        // Shift history window & insert current frame
        for (int i = HISTORY_SIZE - 1; i > 0; i--) {
            history[i] = history[i - 1];
        }
        history[0] = current;
        if (history_count < HISTORY_SIZE) {
            history_count++;
        }

        // Parse sequence number to decide if we should send history
        uint32_t seq = ntohl(current.seq);

        if (seq % 2 != 0 && history_count > 1) {
            // Send 2 frames (current + 1 history frame)
            sendto(sock_out, history, 2 * FRAME_SIZE, 0,
                   (struct sockaddr *)&addr_out, sizeof(addr_out));
        } else {
            // Send only the current frame to keep overhead low
            sendto(sock_out, &current, FRAME_SIZE, 0,
                   (struct sockaddr *)&addr_out, sizeof(addr_out));
        }

    }

    close(sock_in);
    close(sock_out);
    return 0;
}