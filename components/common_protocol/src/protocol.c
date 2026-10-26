#include "protocol.h"
#include <string.h>

uint8_t calculate_checksum(const esp_now_packet_t *pkt) {
    if (!pkt) return 0;
    uint8_t c = pkt->sender_id ^ pkt->target_id ^ pkt->msg_type ^ pkt->payload_len;
    for (int i = 0; i < pkt->payload_len && i < MAX_PAYLOAD_LEN; i++) {
        c ^= pkt->payload[i];
    }
    return c;
}

bool validate_packet(const esp_now_packet_t *pkt) {
    if (!pkt) {
        return false;
    }
    if (pkt->magic != ESP_NOW_PAYLOAD_MAGIC) {
        return false;
    }
    if (pkt->payload_len > MAX_PAYLOAD_LEN) {
        return false;
    }
    return (calculate_checksum(pkt) == pkt->checksum);
}
