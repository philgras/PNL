/*
 * pnl_buffer.h
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_BUFFER_H_
#define SRC_PNL_BUFFER_H_

#include <stddef.h>


/*
 * BUFFER
 */
typedef struct {

    char *data;
    char *position;
    size_t used;
    size_t size;


} pnl_buffer_t;

static inline
void pnl_buffer_init(pnl_buffer_t *buf) {
    buf->data = NULL;
    buf->position = NULL;
    buf->used = 0;
    buf->size = 0;
}


static inline
void pnl_buffer_set_input(pnl_buffer_t *buf, char *data, size_t size) {
    buf->data = data;
    buf->position = buf->data;
    buf->size = size;
    buf->used = 0;
}

static inline
void pnl_buffer_set_output(pnl_buffer_t *buf, char *data, size_t size) {
    buf->data = data;
    buf->position = buf->data;
    buf->used = buf->size = size;

}

static inline
char *pnl_buffer_get_position(pnl_buffer_t *buf) {
    return buf->position;
}

static inline
char *pnl_buffer_set_position(pnl_buffer_t *buf, char *position) {
    if (position >= buf->data && position <= buf->data + buf->used) {
        buf->position = position;
    }
    return buf->position;
}


static inline
void pnl_buffer_reset_position(pnl_buffer_t *buf) {
    buf->position = buf->data;
}


static inline
size_t pnl_buffer_get_used(const pnl_buffer_t *buf) {
    return buf->used;
}

static inline
char *pnl_buffer_used_end(pnl_buffer_t *buf) {
    return buf->data + buf->used;
}

static inline
void pnl_buffer_set_used(pnl_buffer_t *buf, size_t size) {
    if (size <= buf->size) {
        buf->used = size;
    }
}

static inline
void pnl_buffer_add_used(pnl_buffer_t *buf, size_t size) {
    if (buf->used + size <= buf->size) {
        buf->used += size;
    }
}

static inline
size_t pnl_buffer_get_space(const pnl_buffer_t *buf) {
    return buf->size - buf->used;
}

static inline
void pnl_buffer_clear(pnl_buffer_t *buf) {
    buf->position = buf->data;
    buf->used = 0;
}


#endif /* SRC_PNL_BUFFER_H_ */
