/*
 * Protocol encoder - converts JSON API response to binary satellite frames
 */

#ifndef SAT_ENCODER_H
#define SAT_ENCODER_H

#include <stdint.h>
#include <stddef.h>

struct cJSON;

/*
 * Encode a JSON status response into a binary satellite protocol frame.
 * Returns frame length, or -1 on error.
 * buf must be at least SAT_MAX_FRAME_SIZE bytes.
 */
int encoder_json_to_frame(const struct cJSON *status_json,
                          uint8_t *buf, size_t buf_size,
                          uint16_t *seq);

/*
 * Encode a heartbeat frame.
 * Returns frame length, or -1 on error.
 */
int encoder_heartbeat_frame(uint8_t *buf, size_t buf_size, uint16_t *seq);

#endif /* SAT_ENCODER_H */
