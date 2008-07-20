/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * ALSA SEQ < - > JACK MIDI bridge
 *
 * Copyright (c) 2006,2007 Dmitry S. Baikov <c0ff@konstruktiv.org>
 * Copyright (c) 2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef STRUCTS_H__FD2CC895_411F_4ADE_9200_50FE395EDB72__INCLUDED
#define STRUCTS_H__FD2CC895_411F_4ADE_9200_50FE395EDB72__INCLUDED

#define JACK_INVALID_PORT NULL

#define MAX_PORTS  64
#define MAX_EVENT_SIZE 1024
#define MAX_CLIENTS 64

#define PORT_HASH_BITS 4
#define PORT_HASH_SIZE (1 << PORT_HASH_BITS)

typedef struct a2j_port * a2j_port_hash_t[PORT_HASH_SIZE];

struct a2j;

struct a2j_port
{
  struct a2j_port * next;
  struct a2j * a2j_ptr;
  bool is_dead;
  char name[64];
  snd_seq_addr_t remote;
  jack_port_t * jack_port;

  jack_ringbuffer_t * early_events; // alsa_midi_event_t + data
  int64_t last_out_time;

  void * jack_buf;
};

struct a2j_stream
{
  snd_midi_event_t *codec;

  jack_ringbuffer_t *new_ports;

  a2j_port_hash_t port_hash;
};

struct a2j_jack_client
{
  char name[64];
  jack_client_t * client;
  struct a2j * a2j_ptr;
};

struct a2j
{
  const char * jack_server_name;
  struct a2j_jack_client jack_clients[MAX_CLIENTS];

  snd_seq_t *seq;
  int client_id;
  int port_id;
  int queue;

  bool keep_walking;

  pthread_t port_thread;
  sem_t port_sem;
  jack_ringbuffer_t *port_add; // snd_seq_addr_t
  jack_ringbuffer_t *port_del; // struct a2j_port*

  struct a2j_stream stream[2];

  bool export_hw_ports;
};

#define NSEC_PER_SEC ((int64_t)1000*1000*1000)

struct a2j_process_info
{
  int dir;
  jack_nframes_t nframes;
  jack_nframes_t period_start;
  jack_nframes_t sample_rate;
  jack_nframes_t cur_frames;
  int64_t alsa_time;
};

struct a2j_alsa_midi_event
{
  int64_t time;
  int size;
};

#define PORT_INPUT    0
#define PORT_OUTPUT   1

typedef void (*port_jack_func)(struct a2j * self, struct a2j_port * port, struct a2j_process_info * info);

struct a2j_port_type
{
  int alsa_mask;
  int jack_caps;
  port_jack_func jack_func;
};

extern struct a2j_port_type g_port_type[2];

#endif /* #ifndef STRUCTS_H__FD2CC895_411F_4ADE_9200_50FE395EDB72__INCLUDED */
