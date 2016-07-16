/*
 * pnl_buffer.h
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_BUFFER_H_
#define SRC_PNL_BUFFER_H_

#include "pnl_common.h"
#include <string.h>
#include <stddef.h>

/*
 * BUFFER
 */
typedef struct{

	char* data;
	char* position;
	size_t used;
	size_t size;


} pnl_buffer_t;

static inline
void pnl_buffer_init(pnl_buffer_t* buf){
	buf->data = NULL;
	buf->position = NULL;
	buf->used = 0;
	buf->size = 0;
}

static inline
int pnl_buffer_is_allocated(const pnl_buffer_t* buf){
	return buf->data != NULL;
}

static inline
int pnl_buffer_alloc(pnl_buffer_t* buf, size_t size){
	char* new_data = pnl_malloc(size);
	if(new_data== NULL){
		return PNL_ERR;
	}else{
		buf->size = size;
		buf->used = 0;
		buf->position = new_data;
		buf->data = new_data;
	}
	return PNL_OK;
}

static inline
void pnl_buffer_free(pnl_buffer_t* buf){
	pnl_free(buf->data);
	pnl_buffer_init(buf);
}

static inline
char* pnl_buffer_get_position(pnl_buffer_t* buf){
	 return buf->position;
}

static inline
void pnl_buffer_set_position(pnl_buffer_t* buf, char* position){
	if(position >= buf->data && position <= buf->data + buf->used){
		buf->position  = position;
	}
}


static inline
void pnl_buffer_reset_position(pnl_buffer_t* buf){
	 buf->position = buf->data;
}

static inline
size_t pnl_buffer_get_size(const pnl_buffer_t* buf){
	 return buf->size;
}

static inline
size_t pnl_buffer_get_used(const pnl_buffer_t* buf){
	 return buf->used;
}

static inline
char* pnl_buffer_begin(pnl_buffer_t* buf){
	return buf->data;
}

static inline
const char* pnl_buffer_cbegin(const pnl_buffer_t* buf){
	return buf->data;
}

static inline
char* pnl_buffer_end(pnl_buffer_t* buf){
	return buf->data + buf->size;
}

static inline
char* pnl_buffer_used_end(pnl_buffer_t* buf){
	return buf->data + buf->used;
}

static inline
void pnl_buffer_set_used( pnl_buffer_t* buf, size_t size){
	if(size <= buf->size){
		buf->used = size;
	}
}

static inline
void pnl_buffer_add_used( pnl_buffer_t* buf, size_t size){
	if(buf->used + size <= buf->size){
		buf->used += size;
	}
}

static inline
size_t pnl_buffer_get_space(const pnl_buffer_t* buf){
	return buf->size - buf->used;
}

static inline
void pnl_buffer_clear(pnl_buffer_t* buf){
	buf->position = buf->data;
	buf->used = 0;
}

static inline
void pnl_buffer_write(pnl_buffer_t* buf,const char* data, size_t length){
	size_t space = pnl_buffer_get_space(buf);
	memcpy(buf->data+buf->used,data,length>space?space:length);
	buf->used+= length>space?space:length;
}

static inline
const char* pnl_buffer_read(const pnl_buffer_t* buf, size_t size){
	const char* ptr = buf->data;
	if(buf->position+size > buf->data + buf->used ){
		ptr = NULL;
	}
	return ptr;
}


#endif /* SRC_PNL_BUFFER_H_ */
