/* Copyright (C) 2004 Mark Seaborn

   This file is part of Plash, the Principle of Least Authority Shell.

   Plash is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Plash is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Plash; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "comms.h"
#include "region.h"
#include "filesysobj.h"
#include "cap-protocol.h"


#define CAPP_ID_SHIFT 8
#define CAPP_NAMESPACE_MASK 0xff
#define CAPP_NAMESPACE_RECEIVER			0
#define CAPP_NAMESPACE_SENDER			1
#define CAPP_NAMESPACE_SENDER_SINGLE_USE	2
#define CAPP_WIRE_ID(namespace, id)	(((id) << CAPP_ID_SHIFT) | (namespace))

/* Messages:
   "Invk" cap/int no_cap_args/int cap_args data
   "Drop" cap/int

   Meanings of capability IDs:
    * Invoke destination and Drop:  may only be NAMESPACE_RECEIVER
    * Invoke argument:  may be RECEIVER or SENDER.
      SENDER_SINGLE_USE uses the same ID namespace as SINGLE_USE, but
      invoking the capability implies dropping it too.

   The references that are exported on a connection when it is created
   are never single-use.

   Possible changes: make "Drop" take an array of references.
   A common pattern is to drop a reference immediately after invoking it.
   (This is done with return capabilities.)  It may be more efficient
   to combine the invocation and the drop, so that at least they're sent
   with the same "sendmsg" call.  This could reduce the number of
   context switches.
*/

/* Conventions for message contents:
   "Call" msg + CAP (used as return capability)
   "ECon" (connection broken)
   "Fail" ...
   "Okay" ...
*/

/* If true, the connection will be severed if the protocol is violated,
   ie. if any illegal IDs are seen. */
#define STRICT_PROTOCOL 1

#ifndef IN_RTLD

#define DO_LOG
#define MOD_DEBUG 0
#define MOD_LOG_ERRORS 1
#define MOD_MSG "cap-protocol: "
#define LOG stderr
#define PRINT_PID (fprintf(LOG, "[%i] ", getpid()))

#ifndef IN_LIBC
#define MOD_DUMP_ON_VIOLATION
#endif

#endif


struct export_entry {
  char used;
  char single_use;
  union {
    cap_t cap; /* owning reference */
    int next; /* may be equal to export_size */
  } x;
};

struct connection_list {
  /* A list of all connections is kept. */
  int head;
  struct connection *prev, *next;
};

struct connection {
  /* Must come first. */
  struct connection_list l;

  const char *name; /* Used for debugging; not freed */
  int conn_id; /* Used for identifying the connection in debugging info */

  /* If the connection is closed (eg. because it was dropped by the other
     end), comm is set to null and the FD is closed.  The export table is
     deallocated.  The connection is removed from the list.  However, the
     connection needs to be kept around because imported objects will
     refer to it.  We can only deallocate the connection object when
     `import_count' reaches zero. */
  struct comm *comm;
  int sock_fd; /* for sending messages */
  int ready_to_read;

  /* Exported objects. */
  struct export_entry *export;
  int export_size; /* size of the `export' array */
  int export_count; /* number of export entries with `used' set */
  /* This starts a linked list of free slots: */
  int export_next; /* may be equal to export_size */

  /* Number of remote_objects referring to this connection. */
  int import_count;
};

struct c_server_state {
  struct connection_list list;
  int total_ready_to_read;
  /* This is the sum of the export_counts of the connections: */
  int total_export_count;

  /* Arguments to select(): */
  int max_fd;
  fd_set set;
};

static struct c_server_state server_state =
  { .list = { .head = 1,
	      .prev = (struct connection *) &server_state.list,
	      .next = (struct connection *) &server_state.list },
    .total_ready_to_read = 0,
    .total_export_count = 0,
    .max_fd = 0
  };

extern struct filesys_obj_vtable remote_obj_vtable;
struct remote_obj {
  struct filesys_obj hdr;
  struct connection *conn; /* null for a single-use capability that has been used */
  char single_use;
  int id;
};


struct cap_seq caps_empty = { 0, 0 };


/* Sets up the arguments to select().  Needs to be called every time the
   process list is changed. */
static void init_fd_set(struct c_server_state *state)
{
  struct connection *node;
  state->max_fd = 0;
  FD_ZERO(&state->set);
  for(node = state->list.next; !node->l.head; node = node->l.next) {
    int fd = node->comm->sock;
    if(state->max_fd < fd+1) state->max_fd = fd+1;
    FD_SET(fd, &state->set);
  }
}

static void shut_down_connection(struct connection *conn)
{
  int i;
  struct export_entry *export = conn->export;
  int export_size = conn->export_size;
  assert(conn->comm); /* Connection should not have been shut down already */

  /* Close socket and connection. */
  if(close(conn->sock_fd) < 0) { /* perror("close"); */ }
  comm_free(conn->comm);
  server_state.total_ready_to_read -= conn->ready_to_read;

  /* Remove from list. */
  conn->l.prev->l.next = conn->l.next;
  conn->l.next->l.prev = conn->l.prev;
  init_fd_set(&server_state);

  server_state.total_export_count -= conn->export_count;

  if(conn->import_count > 0) {
    /* Mark connection as shut down (but we don't deallocate it yet). */
    conn->comm = 0;
    /* Zero other details just in case: */
    conn->l.prev = 0;
    conn->l.next = 0;
    conn->sock_fd = 0;
    conn->export = 0;
    conn->export_size = 0;
    conn->export_count = 0;
    conn->export_next = 0;
  }
  else {
    free(conn);
  }

  /* Free exported references. */
  /* This is done last, having shut down the connection, because it may
     result in arbitrary code being called.  For example, dropping these
     references can result in other references being dropped, which could
     result in an attempt to call shut_down_connection() on this same
     connection recursively. */
  for(i = 0; i < export_size; i++) {
    if(export[i].used) filesys_obj_free(export[i].x.cap);
  }
  free(export);
}

static void violation(struct connection *conn, seqf_t msg)
{
  if(STRICT_PROTOCOL) {
#ifdef DO_LOG
    if(MOD_LOG_ERRORS) {
      PRINT_PID;
      fprintf(LOG, MOD_MSG "[fd %i] %s: protocol violation in strict mode: shutting down connection\n", conn->sock_fd, conn->name);
    }
#endif
    shut_down_connection(conn);
  }
#ifdef MOD_DUMP_ON_VIOLATION
  PRINT_PID;
  fprintf(LOG, MOD_MSG "offending message:\n");
  fprint_data(LOG, msg);
#endif
}

/* Ensure that the export table has at least one free slot.
   If it needs resizing, it will give it `slack' new free slots. */
static void resize_export_table(struct connection *conn, int slack)
{
  assert(slack >= 1);
  if(conn->export_next >= conn->export_size) {
    int i;
    int new_size = conn->export_size + slack;
    struct export_entry *export_new =
      amalloc(new_size * sizeof(struct export_entry));
    memcpy(export_new, conn->export,
	   conn->export_size * sizeof(struct export_entry));
    for(i = conn->export_size; i < new_size; i++) {
      export_new[i].used = 0;
      export_new[i].x.next = i + 1;
    }
    free(conn->export);
    conn->export = export_new;
    conn->export_size = new_size;
  }
  assert(conn->export_next < conn->export_size);
}

/* Returns non-zero if the connection is invalid on exit. */
static int decr_import_count(struct connection *conn)
{
  assert(conn->import_count > 0);
  conn->import_count--;
  /* If this was the last object, close down the connection.  There's no
     point in sending the "Drop" message first. */
  if(conn->import_count == 0 && conn->export_count == 0) {
    if(conn->comm) {
#ifdef DO_LOG
      if(MOD_DEBUG) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: free last reference: dropping connection\n", conn->sock_fd, conn->name);
      }
#endif
      shut_down_connection(conn);
    }
    else {
#ifdef DO_LOG
      if(MOD_DEBUG) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "free: freeing last reference to dropped connection \"%s\"\n", conn->name);
      }
#endif
      free(conn);
    }
    return 1;
  }
  return 0;
}

void remote_obj_free(struct filesys_obj *obj1)
{
  struct remote_obj *obj = (void *) obj1;
  struct connection *conn = obj->conn;
  if(!conn) return; /* Single-use capability has already been used. */
  if(decr_import_count(conn)) return;
  if(conn->comm) {
    region_t r = region_make();
#ifdef DO_LOG
    if(MOD_DEBUG) {
      PRINT_PID;
      fprintf(LOG, MOD_MSG "[fd %i] %s: free: dropping reference 0x%x\n", obj->conn->sock_fd, obj->conn->name, obj->id);
    }
#endif
    comm_send(r, conn->sock_fd,
	      cat2(r, mk_string(r, "Drop"),
		   mk_int(r, CAPP_WIRE_ID(CAPP_NAMESPACE_RECEIVER, obj->id))),
	      fds_empty);
    region_free(r);
  }
}

/* For debugging: */
#ifdef DO_LOG
static void print_msg(FILE *fp, seqf_t data_orig)
{
  {
    seqf_t data = data_orig;
    int ok = 1;
    int method;
    m_str(&ok, &data, "Call");
    m_int(&ok, &data, &method);
    if(ok) {
      char *x = alloca(sizeof(int) + 1);
      memcpy(x, data_orig.data + 4, sizeof(int));
      x[sizeof(int)] = 0;
      fprintf(fp, "Call %s", x);
      return;
    }
  }
  {
    seqf_t data = data_orig;
    int ok = 1;
    int method;
    m_int(&ok, &data, &method);
    if(ok) {
      char *x = alloca(sizeof(int) + 1);
      memcpy(x, data_orig.data, sizeof(int));
      x[sizeof(int)] = 0;
      fprintf(fp, "%s", x);
      return;
    }
  }
  fprintf(fp, "???");
}
static void print_msgt(FILE *fp, seqt_t data)
{
  region_t r = region_make();
  print_msg(fp, flatten_reuse(r, data));
  region_free(r);
}
#endif

/* Takes the capability and FD arguments as owning references. */
void remote_obj_invoke(struct filesys_obj *obj, struct cap_args args)
{
  struct remote_obj *dest = (void *) obj;
  int i;
  int *caps;
  region_t r;
  struct connection *conn = dest->conn;
  if(!conn) {
    /* Single-use capability has already been used. */
#ifdef DO_LOG
    if(MOD_DEBUG) {
      PRINT_PID;
      if(dest->single_use) {
	fprintf(LOG, MOD_MSG "send tried on used single-use cap (or on closed connection), id 0x%x (connection unknown): ", dest->id);
      }
      else {
	fprintf(LOG, MOD_MSG "send tried on closed connection, id 0x%x (connection unknown): ", dest->id);
      }
      print_msgt(LOG, args.data);
      fprintf(LOG, "\n");
    }
#endif
    close_fds(args.fds);
    caps_free(args.caps);
    return;
  }
  /* If the connection has been closed, this fails silently.  You can
     detect the failure because the refcounts of the arguments will
     not be incremented, so the return continuation would get freed. */
  if(!conn->comm) {
#ifdef DO_LOG
    if(MOD_DEBUG) {
      PRINT_PID;
      fprintf(LOG, MOD_MSG "[fd -] %s: send tried on closed connection, id 0x%x: ", conn->name, dest->id);
      print_msgt(LOG, args.data);
      fprintf(LOG, "\n");
    }
#endif
    decr_import_count(conn); /* this simply allows the `struct connection' to be freed */
    dest->conn = 0;
    close_fds(args.fds);
    caps_free(args.caps); /* must come last */
    return;
  }
  r = region_make();

#ifdef DO_LOG
  if(MOD_DEBUG) {
    PRINT_PID;
    fprintf(LOG, MOD_MSG "[fd %i] %s: send on id 0x%x: ", conn->sock_fd, conn->name, dest->id);
    print_msgt(LOG, args.data);
    fprintf(LOG, "\n");
  }
#endif

  caps = region_alloc(r, args.caps.size * sizeof(int));
  for(i = 0; i < args.caps.size; i++) {
    cap_t c = args.caps.caps[i];
    if(c->vtable == &remote_obj_vtable &&
       ((struct remote_obj *) c)->conn == conn) {
      /* The `else' case would also work for this case, but would create
	 a proxy object for an object that resides on the other end,
	 which would be silly. */
      int id = ((struct remote_obj *) c)->id;
      caps[i] = CAPP_WIRE_ID(CAPP_NAMESPACE_RECEIVER, id);
    }
    else {
      int id;
      resize_export_table(conn, (args.caps.size - i) + 5);
      id = conn->export_next;
      conn->export_next = conn->export[id].x.next;
      conn->export_count++;
      server_state.total_export_count++;
      conn->export[id].used = 1;
      conn->export[id].single_use = c->vtable->single_use;
      conn->export[id].x.cap = c;
      c->refcount++; /* this is decremented later */
      caps[i] = CAPP_WIRE_ID(c->vtable->single_use
			     ? CAPP_NAMESPACE_SENDER_SINGLE_USE
			     : CAPP_NAMESPACE_SENDER, id);
    }
  }
  comm_send(r, dest->conn->sock_fd,
	    cat5(r, mk_string(r, "Invk"),
		 mk_int(r, CAPP_WIRE_ID(CAPP_NAMESPACE_RECEIVER, dest->id)),
		 mk_int(r, args.caps.size),
		 mk_leaf2(r, (void *) caps, args.caps.size * sizeof(int)),
		 args.data),
	    args.fds);
  region_free(r);

  if(dest->single_use) {
    decr_import_count(conn);
    dest->conn = 0;
  }
  close_fds(args.fds);
  caps_free(args.caps); /* Must come last */
}

/* Returns an owning reference. */
/* Returns 0 for an error. */
static cap_t lookup_id(struct connection *conn, int full_id)
{
  int id = full_id >> CAPP_ID_SHIFT;
  switch(full_id & CAPP_NAMESPACE_MASK) {
    case CAPP_NAMESPACE_RECEIVER:
      if(id >= 0 && id < conn->export_size && conn->export[id].used) {
	cap_t c = conn->export[id].x.cap;
	c->refcount++;
	return c;
      }
#ifdef DO_LOG
      if(MOD_LOG_ERRORS) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: bad index in argument id: 0x%x\n", conn->sock_fd, conn->name, full_id);
      }
#endif
      return 0; /* Error */
    case CAPP_NAMESPACE_SENDER:
    case CAPP_NAMESPACE_SENDER_SINGLE_USE:
      {
        struct remote_obj *obj = amalloc(sizeof(struct remote_obj));
        obj->hdr.refcount = 1;
        obj->hdr.vtable = &remote_obj_vtable;
        obj->conn = conn;
	obj->single_use =
	  (full_id & CAPP_NAMESPACE_MASK) == CAPP_NAMESPACE_SENDER_SINGLE_USE;
        obj->id = id;
	conn->import_count++;
        return (cap_t) obj;
      }
    default:
#ifdef DO_LOG
      if(MOD_LOG_ERRORS) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: bad namespace in argument id: 0x%x\n", conn->sock_fd, conn->name, full_id);
      }
#endif
      return 0;
  }
}

/* Called as a result of a "Drop" message or an "Invoke" message on a
   single-use capability. */
static void remove_exported_id(struct connection *conn, int id)
{
  conn->export[id].x.next = conn->export_next;
  conn->export[id].used = 0;
  conn->export_next = id;
  assert(conn->export_count > 0);
  conn->export_count--;
  server_state.total_export_count--;
  /* Shutting down will free the export table, but not the object that
     was just removed from the export table, which becomes owned by
     the caller. */
  if(conn->export_count == 0 && conn->import_count == 0) {
    /* This could happen if there's a mismatch between what
       the two ends think they've imported and exported. */
#ifdef DO_LOG
    if(MOD_LOG_ERRORS) {
      PRINT_PID;
      fprintf(LOG, MOD_MSG "[fd %i] %s: why didn't the other end drop the connection?\n", conn->sock_fd, conn->name);
    }
#endif
    shut_down_connection(conn);
  }
}

/* May render `conn' invalid. */
static void handle_msg(struct connection *conn, seqf_t data_orig, fds_t fds)
{
  {
    seqf_t data = data_orig;
    int dest_id, no_caps;
    seqf_t caps_data;
    int ok = 1;
    m_str(&ok, &data, "Invk");
    m_int(&ok, &data, &dest_id);
    m_int(&ok, &data, &no_caps);
    m_block(&ok, &data, no_caps * sizeof(int), &caps_data);
    if(ok) {
      int *caps = (void *) caps_data.data;
      int id = dest_id >> CAPP_ID_SHIFT;
#ifdef DO_LOG
      if(MOD_DEBUG) {
	int i;
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: got invoke 0x%x: ", conn->sock_fd, conn->name, dest_id);
	print_msg(LOG, data);
	fprintf(LOG, " with %i fds", fds.count);
	for(i = 0; i < fds.count; i++) {
	  fprintf(LOG, " %i", fds.fds[i]);
	}
	fprintf(LOG, "\n");
      }
#endif
      switch(dest_id & CAPP_NAMESPACE_MASK) {
        case CAPP_NAMESPACE_RECEIVER:
          if(0 <= id && id < conn->export_size && conn->export[id].used) {
	    region_t r = region_make();
            cap_t dest = conn->export[id].x.cap;
	    int single_use = conn->export[id].single_use;
	    seqf_t data_copy;
	    fds_t fds_copy;

            cap_t *cap_args = region_alloc(r, no_caps * sizeof(cap_t));
	    int i;
            for(i = 0; i < no_caps; i++) {
              cap_t c = lookup_id(conn, caps[i]);
              if(!c) {
		/* Error (message already printed) */
		int j;
		for(j = 0; j < i; j++) filesys_obj_free(cap_args[j]);
		region_free(r);
		violation(conn, data_orig);
		return;
	      }
              cap_args[i] = c;
            }

	    /* Need to copy the data and FD references because the
	       storage may be reused by comms.c. */
	    {
	      int *f = region_alloc(r, fds.count * sizeof(int));
	      memcpy(f, fds.fds, fds.count * sizeof(int));
	      fds_copy.fds = f;
	      fds_copy.count = fds.count;
	      data_copy = region_dup_seqf(r, data);
	    }

	    /* If the capability is single use, we need to remove it from
	       the export table before invoking it, because invoking it
	       can execute arbitrary code which may cause handle_msg to
	       be called for this connection, or it may cause `conn' to
	       become invalid. */
	    /* If the capability is single use, we now own the reference
	       to it that the export table contained.  If not, we need to
	       claim ownership to it by incrementing the reference count:
	       invoking it could cause the export table to be freed,
	       destroying `dest', but we need to make sure that `dest' is
	       valid throughout its invocation. */
	    if(single_use) { remove_exported_id(conn, id); }
	    else { dest->refcount++; }
	    filesys_obj_check(dest);
            dest->vtable->cap_invoke(dest,
				     cap_args_make
				       (mk_leaf(r, data_copy),
					cap_seq_make(cap_args, no_caps),
					fds_copy));
            region_free(r);
	    filesys_obj_free(dest);
          }
          else {
#ifdef DO_LOG
            if(MOD_LOG_ERRORS) {
	      PRINT_PID;
	      fprintf(LOG, MOD_MSG "[fd %i] %s: bad index in destination id: 0x%x\n", conn->sock_fd, conn->name, dest_id);
	    }
#endif
	    violation(conn, data_orig);
          }
          return;
        default:
#ifdef DO_LOG
          if(MOD_LOG_ERRORS) {
	    PRINT_PID;
	    fprintf(LOG, MOD_MSG "[fd %i] %s: bad namespace in destination id: 0x%x\n", conn->sock_fd, conn->name, dest_id);
	  }
#endif
	  violation(conn, data_orig);
          return;
      }
    }
  }
  {
    seqf_t data = data_orig;
    int dest_id;
    int ok = 1;
    m_str(&ok, &data, "Drop");
    m_int(&ok, &data, &dest_id);
    m_end(&ok, &data);
    if(ok && fds.count == 0) {
      int id = dest_id >> CAPP_ID_SHIFT;
#ifdef DO_LOG
      if(MOD_DEBUG) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: got drop 0x%x\n", conn->sock_fd, conn->name, dest_id);
      }
#endif
      switch(dest_id & CAPP_NAMESPACE_MASK) {
        case CAPP_NAMESPACE_RECEIVER:
	  if(0 <= id && id < conn->export_size && conn->export[id].used) {
	    cap_t c = conn->export[id].x.cap;
	    remove_exported_id(conn, id);
	    /* This needs to be done last because it may call arbitrary
	       finalisation code which may render `conn' invalid. */
	    filesys_obj_free(c);
	  }
	  else {
#ifdef DO_LOG
	    if(MOD_LOG_ERRORS) {
	      PRINT_PID;
	      fprintf(LOG, MOD_MSG "[fd %i] %s: bad index in dropped id: 0x%x\n", conn->sock_fd, conn->name, dest_id);
	    }
#endif
	    violation(conn, data_orig);
	  }
	  return;
        default:
#ifdef DO_LOG
          if(MOD_LOG_ERRORS) {
	    PRINT_PID;
	    fprintf(LOG, MOD_MSG "[fd %i] %s: bad namespace in dropped id: 0x%x\n", conn->sock_fd, conn->name, dest_id);
	  }
#endif
	  violation(conn, data_orig);
	  return;
      }
    }
  }
#ifdef DO_LOG
  if(MOD_LOG_ERRORS) {
    PRINT_PID;
    fprintf(LOG, MOD_MSG "[fd %i] %s: unknown message\n", conn->sock_fd, conn->name);
  }
#endif
  violation(conn, data_orig);
  close_fds(fds);
}

static int next_conn_id = 0;

/* Creates a new connection given a socket file descriptor.
   Exports the given objects on the connection, and returns an array
   containing object references for `import_count' imported objects. */
/* `export' contains non-owned references.  Returns owned references. */
cap_t *cap_make_connection(region_t r, int sock_fd,
			   cap_seq_t export, int import_count,
			   const char *name)
{
  int i;
  cap_t *import;
  struct connection *conn;
  assert(sock_fd >= 0);
  if(import_count == 0 && export.size == 0) return 0; /* useless connection */
  conn = amalloc(sizeof(struct connection));

#ifdef DO_LOG
  if(MOD_DEBUG) {
    PRINT_PID;
    fprintf(LOG, MOD_MSG "%s: creating connection from socket %i: import %i, export %i\n", name, sock_fd, import_count, export.size);
  }
#endif

  conn->name = name;
  conn->conn_id = next_conn_id++;

  conn->comm = comm_init(sock_fd);
  conn->sock_fd = sock_fd;
  conn->ready_to_read = 0;

  conn->export_size = export.size;
  conn->export_count = export.size;
  conn->export_next = export.size;
  conn->export = amalloc(export.size * sizeof(struct export_entry));
  for(i = 0; i < export.size; i++) {
    filesys_obj_check(export.caps[i]);
    conn->export[i].used = 1;
    conn->export[i].single_use = 0;
    conn->export[i].x.cap = inc_ref(export.caps[i]);
  }

  /* Insert into list of connections. */
  conn->l.head = 0;
  conn->l.prev = server_state.list.prev;
  conn->l.next = (struct connection *) &server_state.list;
  server_state.list.prev->l.next = conn;
  server_state.list.prev = conn;
  init_fd_set(&server_state);

  server_state.total_export_count += export.size;

  /* Create imported object references. */
  conn->import_count = 0; /* This will be incremented by lookup_id(). */
  import = region_alloc(r, import_count * sizeof(cap_t));
  for(i = 0; i < import_count; i++) {
    import[i] = lookup_id(conn, CAPP_WIRE_ID(CAPP_NAMESPACE_SENDER, i));
    assert(import[i]);
  }
  assert(conn->import_count == import_count);
  return import;
}

/* This is necessary when forking a new process.  It's not safe to re-use
   any of the sockets in the new process. */
void cap_close_all_connections()
{
  struct c_server_state *state = &server_state;
  /* Shutting down one connection frees its export table, which can call
     arbitrary code, so might result in shutting down of other connections.
     This means that we can only access the head of the connection list
     on each iteration. */
  while(1) {
    struct connection *conn = state->list.next;
    if(conn->l.head) break;
    shut_down_connection(conn);
  }
}

/* Read from a connection's socket and process messages. */
static void listen_on_connection(struct connection *conn)
{
  int r, err;

  server_state.total_ready_to_read -= conn->ready_to_read;
  conn->ready_to_read = 0;

  r = comm_read(conn->comm, &err);
  if(r < 0 && err == EINTR) { /* nothing */ }
  else if(r < 0 && err == EAGAIN) {
#ifdef DO_LOG
    if(MOD_LOG_ERRORS) {
      PRINT_PID;
      fprintf(LOG, MOD_MSG "[fd %i] %s: got EAGAIN on recv: why?\n", conn->sock_fd, conn->name);
    }
#endif
  }
  else if(r < 0 || r == COMM_END) {
#ifdef DO_LOG
    if(r < 0) {
      if(MOD_LOG_ERRORS) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: connection error, errno %i (%s)\n",
		conn->sock_fd, conn->name, err, strerror(err));
      }
    }
    else {
      if(MOD_DEBUG) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "[fd %i] %s: connection end\n", conn->sock_fd, conn->name);
      }
    }
#endif

    /* Close socket and remove process from list */
    shut_down_connection(conn);
  }
  else {
    seqf_t msg;
    fds_t fds;
    conn->import_count++;
    while(conn->comm) {
      r = comm_try_get(conn->comm, &msg, &fds);
      if(r != COMM_AVAIL) break;
      handle_msg(conn, msg, fds);
    }
    decr_import_count(conn);
  }
}

/* run_server_step() needs to be re-entrant.  Handling a message may
   cause an object to wait for another message.  Hence the list may
   change: we can't carry on traversing it, because elements may have
   disappeared.  Furthermore, the input that select() said was ready
   may have now been read. */
/* Returns 0 when there are no connections left, or if it gets an
   error from select() (which shouldn't happen). */
int cap_run_server_step()
{
  struct c_server_state *state = &server_state;
  if(state->list.next->l.head) return 0;

  /*
  PRINT_PID;
  fprintf(LOG, MOD_MSG "connections:\n");
  cap_print_connections_info(stderr);
  */

#ifdef IN_RTLD
  /* In ld.so, select() is not available. */
  listen_on_connection(state->list.next);
  return 1;
#else
  /* See if there is only one active connection.  If so, we don't need
     to use select(), and we save a system call. */
  if(state->list.next->l.next->l.head) {
#ifdef DO_LOG
    if(MOD_DEBUG) {
      PRINT_PID;
      fprintf(LOG, MOD_MSG "[fd %i] %s: run_server_step: only one connection\n", state->list.next->sock_fd, state->list.next->name);
    }
#endif
    listen_on_connection(state->list.next);
    return 1;
  }
  else {
    struct connection *conn;
    if(state->total_ready_to_read == 0) {
      int result;
      fd_set read_fds = state->set;

#ifdef DO_LOG
      if(MOD_DEBUG) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "run_server_step: calling select()\n");
      }
#endif
      result = select(state->max_fd, &read_fds, 0, 0, 0 /* &timeout */);
      if(result < 0 && errno == EINTR) return 1;
      if(result < 0) { perror("select"); return 0; }

      for(conn = state->list.next;
	  !conn->l.head && result > 0;
	  conn = conn->l.next) {
	if(FD_ISSET(conn->comm->sock, &read_fds)) {
	  result--;
	  state->total_ready_to_read += 1 - conn->ready_to_read;
	  conn->ready_to_read = 1;
	}
      }
    }
    assert(state->total_ready_to_read > 0);

    for(conn = state->list.next; !conn->l.head; conn = conn->l.next) {
      if(conn->ready_to_read) {
	/* Move this connection to the end of the list to ensure fairness. */
	/* Remove from the list. */
	conn->l.prev->l.next = conn->l.next;
	conn->l.next->l.prev = conn->l.prev;
	/* Insert at the end. */
	conn->l.prev = state->list.prev;
	conn->l.next = (struct connection *) &state->list;
	state->list.prev->l.next = conn;
	state->list.prev = conn;

	listen_on_connection(conn);
	return 1;
      }
    }
    assert(0); /* Could happen if select() is returning inconsistent info */
    return 0;
  }
#endif
}

void cap_run_server()
{
  // /* While the process list is non-empty... */
  // while(cap_run_server_step()) { /* empty */ }

  while(1) {
    /* If no objects are exported, the server finishes.  Any imports can
       be dropped, since it is assumed they will not be used any more.
       However, if imports remain, this could indicate a reference leak
       (unless they are held by global variables), so a warning is given. */
    if(server_state.total_export_count <= 0) {
#ifdef DO_LOG
      /* If the connection list is non-empty, imports remain. */
      if(!server_state.list.next->l.head && MOD_LOG_ERRORS) {
	PRINT_PID;
	fprintf(LOG, MOD_MSG "warning: no exports, so finished, but imports remain -- possible reference leak\n");
      }
#endif
      break;
    }

    if(!cap_run_server_step()) break;
  }
}



/* Very similar to init_fd_set(). */
void cap_add_select_fds(int *max_fd,
			fd_set *read_fds,
			fd_set *write_fds,
			fd_set *except_fds)
{
  struct c_server_state *state = &server_state;
  struct connection *node;
  for(node = state->list.next; !node->l.head; node = node->l.next) {
    int fd = node->comm->sock;
    if(*max_fd < fd+1) *max_fd = fd+1;
    FD_SET(fd, read_fds);
  }
}

void cap_handle_select_result(fd_set *read_fds,
			      fd_set *write_fds,
			      fd_set *except_fds)
{
  struct c_server_state *state = &server_state;
  struct connection *conn;
  for(conn = state->list.next;
      !conn->l.head;
      conn = conn->l.next) {
    if(FD_ISSET(conn->comm->sock, read_fds)) {
      state->total_ready_to_read += 1 - conn->ready_to_read;
      conn->ready_to_read = 1;
    }
  }
  if(state->total_ready_to_read > 0) {
    for(conn = state->list.next; !conn->l.head; conn = conn->l.next) {
      if(conn->ready_to_read) {
	/* Move this connection to the end of the list to ensure fairness. */
	/* Remove from the list. */
	conn->l.prev->l.next = conn->l.next;
	conn->l.next->l.prev = conn->l.prev;
	/* Insert at the end. */
	conn->l.prev = state->list.prev;
	conn->l.next = (struct connection *) &state->list;
	state->list.prev->l.next = conn;
	state->list.prev = conn;

	listen_on_connection(conn);
	return;
      }
    }
    assert(0); /* Could happen if select() is returning inconsistent info */
  }
}



/* For debugging: */
#ifndef IN_RTLD
void cap_print_cap(FILE *fp, cap_t obj)
{
  fprintf(fp, "%p=", obj);
  if(obj->vtable == &remote_obj_vtable) {
    struct remote_obj *r = (void *) obj;
    if(r->conn) {
      fprintf(fp, "remote_obj(conn=%i/`%s', id=%i%s)",
	      r->conn->conn_id, r->conn->name, r->id,
	      r->single_use ? ", single_use" : "");
    }
    else {
      fprintf(fp, "remote_obj(conn=invalid, id=%i%s)",
	      r->id,
	      r->single_use ? ", single_use" : "");
    }
  }
  else {
    fprintf(fp, "%s", obj->vtable->vtable_name);
  }
}

void cap_print_connections_info(FILE *fp)
{
  struct connection *conn;
  int i = 0;
  if(server_state.list.next->l.head) fprintf(fp, "no connections\n");
  for(conn = server_state.list.next; !conn->l.head; conn = conn->l.next) {
    int j;
    
    fprintf(fp, "  connection #%i, %i/`%s', counts: import=%i, export=%i\n",
	    i, conn->conn_id, conn->name,
	    conn->import_count, conn->export_count);
    i++;

    for(j = 0; j < conn->export_size; j++) {
      if(conn->export[j].used) {
	fprintf(fp, "    export %i%s: ", j,
		conn->export[j].single_use ? " (single-use)" : "");
	cap_print_cap(fp, conn->export[j].x.cap);
	fprintf(fp, "\n");
      }
    }
  }
}
#endif


#include "out-vtable-cap-protocol.h"
