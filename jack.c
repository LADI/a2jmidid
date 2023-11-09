/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * ALSA SEQ < - > JACK MIDI bridge
 *
 * Copyright (c) 2006,2007 Dmitry S. Baikov <c0ff@konstruktiv.org>
 * Copyright (c) 2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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

#include <stdbool.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include "list.h"
#include "structs.h"
#include "jack.h"
#include "log.h"
#include "port_hash.h"
#include "port.h"
#include "a2jmidid.h"
#include "port_thread.h"

static bool g_freewheeling = false;
extern bool g_filter_note_on;

/*
 * =================== Input/output port handling =========================
 */

void a2j_add_ports(struct a2j_stream * str)
{
  struct a2j_port * port_ptr;
  while (jack_ringbuffer_read(str->new_ports, (char *)&port_ptr, sizeof(port_ptr)))
  {
    a2j_debug("jack: inserted port %s", port_ptr->name);
    a2j_port_insert(str->port_hash, port_ptr);
  }
}

/*
 * ============================ Input ==============================
 */
void
a2j_process_incoming (
  struct a2j * self,
  struct a2j_port * port,
  jack_nframes_t nframes)
{
  struct a2j_alsa_midi_event ev;
  //jack_nframes_t now;
  jack_nframes_t one_period;
  char *ev_buf;

  ((void)(nframes));            /* unreferenced parameter */

  /* grab data queued by the ALSA input thread and write it into the JACK
     port buffer. it will delivered during the JACK period that this
     function is called from.
  */

  /* first clear the JACK port buffer in preparation for new data 
   */

  // a2j_debug ("PORT: %s process input", jack_port_name (port->jack_port));

  jack_midi_clear_buffer (port->jack_buf);

  /* now =  */jack_frame_time (self->jack_client);
  one_period = jack_get_buffer_size (self->jack_client);

  while (jack_ringbuffer_peek (port->inbound_events, (char*)&ev, sizeof(ev) ) == sizeof(ev) ) {

    jack_midi_data_t* buf;
    jack_nframes_t offset;

    if (ev.time >= self->cycle_start) {
      break;
    }
    
    //jack_ringbuffer_read_advance (port->inbound_events, sizeof (ev));
    ev_buf = (char *) alloca( sizeof(ev) + ev.size );

    if (jack_ringbuffer_peek (port->inbound_events, ev_buf, sizeof(ev) + ev.size ) != sizeof(ev) + ev.size)
      break;

    offset = self->cycle_start - ev.time;
    if (offset > one_period) {
      /* from a previous cycle, somehow. cram it in at the front */
      offset = 0;
    } else {
      /* offset from start of the current cycle */
      offset = one_period - offset;
    }

    a2j_debug ("event at %d offset %d", ev.time, offset);

    /* make sure there is space for it */
    
    buf = jack_midi_event_reserve (port->jack_buf, offset, ev.size);

    if (buf) {
      /* grab the event */
      memcpy( buf, ev_buf + sizeof(ev), ev.size );
    } else {
      /* throw it away (no space) */
      a2j_error ("threw away MIDI event - not reserved at time %d", ev.time);
    }
    jack_ringbuffer_read_advance (port->inbound_events, sizeof(ev) + ev.size);
    
    a2j_debug("input on %s: sucked %d bytes from inbound at %d", jack_port_name (port->jack_port), ev.size, ev.time);
  }
}

static
void
a2j_port_event(
  struct a2j * self,
  snd_seq_event_t * ev)
{
  const snd_seq_addr_t addr = ev->data.addr;

  if (addr.client == self->client_id)
    return;

  if (ev->type == SND_SEQ_EVENT_PORT_START || ev->type == SND_SEQ_EVENT_PORT_CHANGE) {
    if (jack_ringbuffer_write_space(self->port_add) >= sizeof(addr)) {
      a2j_debug("port_event: add/change %d:%d", addr.client, addr.port);
      jack_ringbuffer_write(self->port_add, (char*)&addr, sizeof(addr));
    } else {
      a2j_error("dropping port_event: add/change %d:%d", addr.client, addr.port);
    }
  } else if (ev->type == SND_SEQ_EVENT_PORT_EXIT) {
    a2j_debug("port_event: del %d:%d", addr.client, addr.port);
    a2j_port_setdead(self->stream[A2J_PORT_CAPTURE].port_hash, addr);
    a2j_port_setdead(self->stream[A2J_PORT_PLAYBACK].port_hash, addr);
  }
}

static
void
a2j_input_event(
  struct a2j * self,
  snd_seq_event_t * alsa_event)
{
  jack_midi_data_t data[MAX_EVENT_SIZE];
  struct a2j_stream *str = &self->stream[A2J_PORT_CAPTURE];
  long size;
  struct a2j_port *port;
  jack_nframes_t now;

  now = jack_frame_time (self->jack_client);
  
  if ((port = a2j_port_get(str->port_hash, alsa_event->source)) == NULL) {
    return;
  }

  /*
   * RPNs, NRPNs, Bank Change, etc. need special handling
   * but seems, ALSA does it for us already.
   */
  snd_midi_event_reset_decode(str->codec);
  if ((size = snd_midi_event_decode(str->codec, data, sizeof(data), alsa_event))<0) {
    return;
  }

  // fixup NoteOn with vel 0
  if (g_filter_note_on && (data[0] & 0xF0) == 0x90 && data[2] == 0x00) {
    data[0] = 0x80 + (data[0] & 0x0F);
    data[2] = 0x40;
  }

  a2j_debug("input: %d bytes at event_frame=%u", (int)size, now);

  if (jack_ringbuffer_write_space(port->inbound_events) >= (sizeof(struct a2j_alsa_midi_event) + size)) {
    struct a2j_alsa_midi_event ev;
    char *ev_charp = (char*) &ev;
    size_t limit;
    size_t to_write = sizeof(ev);

    jack_ringbuffer_data_t vec[2];
    jack_ringbuffer_get_write_vector( port->inbound_events, vec );
    ev.time = now;
    ev.size = size;

    
    limit = (to_write > vec[0].len ? vec[0].len : to_write);
    if( limit ) {
        memcpy( vec[0].buf, ev_charp, limit );
        to_write -= limit;
        ev_charp += limit;
        vec[0].buf += limit;
        vec[0].len -= limit;
    }
    if( to_write ) {
        memcpy( vec[1].buf, ev_charp, to_write );
        vec[1].buf += to_write;
        vec[1].len -= to_write;
    }

    to_write = size;
    ev_charp = (char *)data;
    limit = (to_write > vec[0].len ? vec[0].len : to_write);
    if( limit )
        memcpy( vec[0].buf, ev_charp, limit );
    to_write -= limit;
    ev_charp += limit;
    if( to_write )
        memcpy( vec[1].buf, ev_charp, to_write );

    jack_ringbuffer_write_advance( port->inbound_events, sizeof(ev) + size );
  } else {
    a2j_error ("MIDI data lost (incoming event buffer full): %ld bytes lost", size);
  }

}

/*
 * ============================ Output ==============================
 */

int
a2j_process_outgoing (
  struct a2j * self,
  struct a2j_port * port)
{
  /* collect data from JACK port buffer and queue it for later delivery by ALSA output thread */

  int nevents;
  jack_ringbuffer_data_t vec[2];
  int i;
  size_t written = 0;
  size_t limit;
  struct a2j_delivery_event* dev;
  size_t gap = 0;

  jack_ringbuffer_get_write_vector (self->outbound_events, vec);

  dev = (struct a2j_delivery_event*) vec[0].buf;
  limit = vec[0].len / sizeof (struct a2j_delivery_event);
  nevents = jack_midi_get_event_count (port->jack_buf);

  for (i = 0; (i < nevents) && (written < limit); ++i) {

    jack_midi_event_get (&dev->jack_event, port->jack_buf, i);
    if (dev->jack_event.size <= MAX_JACKMIDI_EV_SIZE)
    {
      dev->time = dev->jack_event.time;
      dev->port = port;
      memcpy( dev->midistring, dev->jack_event.buffer, dev->jack_event.size );
      written++;
      ++dev;
    }
  }

  /* anything left? use the second part of the vector, as much as possible */

  if (i < nevents)
  {
    if (vec[0].len)
    {
        gap = vec[0].len - written * sizeof(struct a2j_delivery_event);
    }

    dev = (struct a2j_delivery_event*) vec[1].buf;

    limit += (vec[1].len / sizeof (struct a2j_delivery_event));

    while ((i < nevents) && (written < limit))
    {
      jack_midi_event_get(&dev->jack_event, port->jack_buf, i);
      if (dev->jack_event.size <= MAX_JACKMIDI_EV_SIZE)
      {
        dev->time = dev->jack_event.time;
        dev->port = port;
        memcpy(dev->midistring, dev->jack_event.buffer, dev->jack_event.size);
        written++;
        ++dev;
      } 
      ++i;
    }
  }

  a2j_debug( "done pushing events: %d ... gap: %d ", (int)written, (int)gap );
  /* clear JACK port buffer; advance ring buffer ptr */

  jack_ringbuffer_write_advance (self->outbound_events, written * sizeof (struct a2j_delivery_event) + gap);

  return nevents;
}

static int
time_sorter (struct a2j_delivery_event * a, struct a2j_delivery_event * b)
{
  if (a->time < b->time) {
    return -1;
  } else if (a->time > b->time) {
    return 1;
  } 
  return 0;
}

void * a2j_alsa_output_thread(void * arg)
{
  struct a2j * self = (struct a2j*) arg;
  struct a2j_stream *str = &self->stream[A2J_PORT_PLAYBACK];
  int i;
  struct list_head evlist;
  struct list_head * node_ptr;
  jack_ringbuffer_data_t vec[2];
  snd_seq_event_t alsa_event;
  struct a2j_delivery_event* ev;
  float sr;
  jack_nframes_t now;
  //int err;
  int limit;

  while (g_keep_alsa_walking) {
    /* first, make a list of all events in the outbound_events FIFO */

    INIT_LIST_HEAD(&evlist);

    jack_ringbuffer_get_read_vector (self->outbound_events, vec);

    a2j_debug ("output thread: got %d+%d events", 
               (vec[0].len / sizeof (struct a2j_delivery_event)),
               (vec[1].len / sizeof (struct a2j_delivery_event)));
    
    ev = (struct a2j_delivery_event*) vec[0].buf;
    limit = vec[0].len / sizeof (struct a2j_delivery_event);
    for (i = 0; i < limit; ++i) {
      list_add_tail(&ev->siblings, &evlist);
      ev++;
    }

    ev = (struct a2j_delivery_event*) vec[1].buf;
    limit = vec[1].len / sizeof (struct a2j_delivery_event);
    for (i = 0; i < limit; ++i) {
      list_add_tail(&ev->siblings, &evlist);
      ev++;
    }

    if (vec[0].len < sizeof(struct a2j_delivery_event) && (vec[1].len == 0)) {
      /* no events: wait for some */
      a2j_debug ("output thread: wait for events");
      sem_wait (&self->io_semaphore);
      a2j_debug ("output thread: AWAKE ... loop back for events");
      continue;
    }

    /* now sort this list by time */

    list_sort(&evlist, struct a2j_delivery_event, siblings, time_sorter);

    /* now deliver */

    sr = jack_get_sample_rate (self->jack_client);

    list_for_each(node_ptr, &evlist)
    {
      ev = list_entry(node_ptr, struct a2j_delivery_event, siblings);

      snd_seq_ev_clear(&alsa_event);
      snd_midi_event_reset_encode(str->codec);
      if (!snd_midi_event_encode(str->codec, (const unsigned char *)ev->midistring, ev->jack_event.size, &alsa_event))
      {
        continue; // invalid event
      }
      
      snd_seq_ev_set_source(&alsa_event, self->port_id);
      snd_seq_ev_set_dest(&alsa_event, ev->port->remote.client, ev->port->remote.port);
      snd_seq_ev_set_direct (&alsa_event);
      
      now = jack_frame_time (self->jack_client);

      ev->time += self->cycle_start;

      a2j_debug ("@ %d, next event @ %d", now, ev->time);
      
      /* do we need to wait a while before delivering? */

      if (ev->time > now) {
        struct timespec nanoseconds;
        jack_nframes_t sleep_frames = ev->time - now;
        float seconds = sleep_frames / sr;

        /* if the gap is long enough, sleep */

        if (seconds > 0.001) {
          nanoseconds.tv_sec = (time_t) seconds;
          nanoseconds.tv_nsec = (long) NSEC_PER_SEC * (seconds - nanoseconds.tv_sec);
          
          a2j_debug ("output thread sleeps for %.2f msec", ((double) nanoseconds.tv_nsec / NSEC_PER_SEC) * 1000.0);

          if (nanosleep (&nanoseconds, NULL) < 0) {
            fprintf (stderr, "BAD SLEEP\n");
            /* do something ? */
          }
        }
      }
      
      /* its time to deliver */
      /* err =  */snd_seq_event_output(self->seq, &alsa_event);
      snd_seq_drain_output (self->seq);
      now = jack_frame_time (self->jack_client);
      a2j_debug("alsa_out: written %d bytes to %s at %d, DELTA = %d", ev->jack_event.size, ev->port->name, now, 
                (int32_t) (now - ev->time));
    }

    /* free up space in the FIFO */
    
    jack_ringbuffer_read_advance (self->outbound_events, vec[0].len + vec[1].len);

    /* and head back for more */
  }

  return (void*) 0;
}

#define a2j_ptr ((struct a2j *)arg)

/* ALSA */

void * a2j_alsa_input_thread(void * arg)
{
  struct a2j * self = arg;
  int npfd;
  struct pollfd * pfd;
  snd_seq_addr_t addr;
  snd_seq_client_info_t * client_info;
  snd_seq_port_info_t * port_info;
  bool initial;
  snd_seq_event_t * event;
  int ret;

  npfd = snd_seq_poll_descriptors_count(self->seq, POLLIN);
  pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
  snd_seq_poll_descriptors(self->seq, pfd, npfd, POLLIN);

  initial = true;
  while (g_keep_alsa_walking)
  {
    if ((ret = poll(pfd, npfd, 1000)) > 0)
    {

      while (snd_seq_event_input (self->seq, &event) > 0)
      {
        if (initial)
        {
          snd_seq_client_info_alloca(&client_info);
          snd_seq_port_info_alloca(&port_info);
          snd_seq_client_info_set_client(client_info, -1);
          while (snd_seq_query_next_client(self->seq, client_info) >= 0)
          {
            addr.client = snd_seq_client_info_get_client(client_info);
            if (addr.client == SND_SEQ_CLIENT_SYSTEM || addr.client == self->client_id)
              continue;
            snd_seq_port_info_set_client(port_info, addr.client);
            snd_seq_port_info_set_port(port_info, -1);
            while (snd_seq_query_next_port(self->seq, port_info) >= 0)
            {
              addr.port = snd_seq_port_info_get_port(port_info);
              a2j_update_port(self, addr, port_info);
            }
          }

          initial = false;
        }

        if (event->source.client == SND_SEQ_CLIENT_SYSTEM)
        {
          a2j_port_event(a2j_ptr, event);
        }
        else
        {
          a2j_input_event(a2j_ptr, event);
        }

        snd_seq_free_event (event);
      }
    }
  }

  return (void*) 0;
}


/* JACK */

static
void
a2j_jack_process_internal(
  struct a2j * self,
  int dir,
  jack_nframes_t nframes)
{
  struct a2j_stream * stream_ptr;
  int i;
  struct a2j_port ** port_ptr_ptr;
  struct a2j_port * port_ptr;
  int nevents = 0;

  stream_ptr = &self->stream[dir];
  a2j_add_ports(stream_ptr);

  // process ports
  for (i = 0 ; i < PORT_HASH_SIZE ; i++)
  {
    port_ptr_ptr = &stream_ptr->port_hash[i];
    while (*port_ptr_ptr != NULL)
    {
      port_ptr = *port_ptr_ptr;

      if (!port_ptr->is_dead)
      {
        port_ptr->jack_buf = jack_port_get_buffer(port_ptr->jack_port, nframes);

        if (dir == A2J_PORT_CAPTURE) {
          a2j_process_incoming (self, port_ptr, nframes);
        } else {
          nevents += a2j_process_outgoing (self, port_ptr);
        }

      } else if (jack_ringbuffer_write_space (self->port_del) >= sizeof(port_ptr)) {

        a2j_debug("jack: removed port %s", port_ptr->name);
        *port_ptr_ptr = port_ptr->next;
        jack_ringbuffer_write(self->port_del, (char*)&port_ptr, sizeof(port_ptr));
        continue;

      }

      port_ptr_ptr = &port_ptr->next;
    }
  }

  /* if we queued up anything for output, tell the output thread in 
     case its waiting for us.
  */

  if (nevents > 0) {
    int sv;
    sem_getvalue (&self->io_semaphore, &sv);
    sem_post (&self->io_semaphore);
  } 
}

static
int
a2j_jack_process(
  jack_nframes_t nframes,
  void * arg)
{
  struct a2j* self = (struct a2j *) arg;

  if (g_freewheeling)
    return 0;

  self->cycle_start = jack_last_frame_time (self->jack_client);

  a2j_jack_process_internal (self, A2J_PORT_CAPTURE, nframes); 
  a2j_jack_process_internal (self, A2J_PORT_PLAYBACK, nframes); 

  return 0;
}

static
void
a2j_jack_freewheel(
  int starting,
  void * arg)
{
  ((void)(arg));                /* unreferenced parameter */
  g_freewheeling = starting;
}

static
void
a2j_jack_shutdown(
  void * arg)
{
  ((void)(arg));                /* unreferenced parameter */
  a2j_warning("JACK server shutdown notification received.");
  g_stop_request = true;
}

#undef a2j_ptr

jack_client_t *
a2j_jack_client_create(
  struct a2j * a2j_ptr,
  const char * client_name,
  const char * server_name)
{
  jack_status_t status;
  jack_client_t * jack_client;

  if (server_name != NULL)
  {
    jack_client = jack_client_open(client_name, JackServerName|JackNoStartServer|JackUseExactName, &status, server_name);
  }
  else
  {
    jack_client = jack_client_open(client_name, JackNoStartServer|JackUseExactName, &status);
  }

  if (!jack_client)
  {
    a2j_error("Cannot create jack client");
    return NULL;
  }

  jack_set_process_callback(jack_client, a2j_jack_process, a2j_ptr);
  jack_set_freewheel_callback(jack_client, a2j_jack_freewheel, NULL);
  jack_on_shutdown(jack_client, a2j_jack_shutdown, NULL);

  return jack_client;
}
