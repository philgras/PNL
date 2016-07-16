/*
 * pnl_stream.h
 *
 *  Created on: 23.06.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_STREAM_H_
#define SRC_PNL_STREAM_H_

#include "pnl_buffer.h"
#include "pnl_common.h"

typedef struct pnl_istream_s pnl_istream_t;
typedef struct pnl_ostream_s pnl_ostream_t;

typedef int (*istream_next_cb)(void* stream_data, void* internal_data, const pnl_buffer_t* );
typedef int (*ostream_next_cb)(const void* stream_data, void* internal_data,  pnl_buffer_t* );

struct pnl_istream_s{
	void* stream_data;
	void* internal_data;
	istream_next_cb next_cb;
	unsigned int closed:1;
};

struct pnl_ostream_s{
	const void* stream_data;
	void* internal_data;
	ostream_next_cb next_cb;
	unsigned int closed:1;
};

static inline
void pnl_istream_init(pnl_istream_t* stream, void* stream_data,void* internal_data, istream_next_cb cb){
	stream->closed = 0;
	stream->internal_data = internal_data;
	stream->stream_data  = stream_data;
	stream->next_cb = cb;
}

static inline
unsigned int pnl_istream_is_closed(pnl_istream_t* stream){
	return stream->closed;
}

static inline
void pnl_istream_next(pnl_istream_t* stream,const pnl_buffer_t* buf){
	int rc = stream->next_cb(stream->stream_data,stream->internal_data,buf);
	if(rc == PNL_ERR){
		stream->closed = 1;
	}
}

static inline
void pnl_ostream_init(pnl_ostream_t* stream, const void* stream_data,void* internal_data, ostream_next_cb cb){
	stream->closed = 0;
	stream->internal_data = internal_data;
	stream->stream_data  = stream_data;
	stream->next_cb = cb;
}

static inline
unsigned int pnl_ostream_is_closed(pnl_ostream_t* stream){
	return stream->closed;
}

static inline
void pnl_ostream_next(pnl_ostream_t* stream,pnl_buffer_t* buf){
	int rc = stream->next_cb(stream->stream_data,stream->internal_data,buf);
	if(rc == PNL_ERR){
		stream->closed = 1;
	}
}

#endif /* SRC_PNL_STREAM_H_ */
