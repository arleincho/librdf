/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdf_stream.c - RDF Statement Stream Implementation
 *
 * $Id$
 *
 * Copyright (C) 2000-2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
 * 
 * 
 */


#include <rdf_config.h>

#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for abort() as used in errors */
#endif

#include <librdf.h>


#ifndef STANDALONE

/* prototypes of local helper functions */
static librdf_statement* librdf_stream_update_current_statement(librdf_stream* stream);


/**
 * librdf_new_stream - Constructor - create a new librdf_stream
 * @world: redland world object
 * @context: context to pass to the stream implementing objects
 * @is_end_method: pointer to function to test for end of stream
 * @next_method: pointer to function to move to the next statement in stream
 * @get_method: pointer to function to get the current statement
 * @finished: pointer to function to finish the stream.
 *
 * Creates a new stream with an implementation based on the passed in
 * functions.  The functions next_statement and end_of_stream will be called
 * multiple times until either of them signify the end of stream by
 * returning NULL or non 0 respectively.  The finished function is called
 * once only when the stream object is destroyed with librdf_free_stream()
 *
 * A mapping function can be set for the stream using librdf_stream_set_map()
 * function which allows the statements generated by the stream to be
 * filtered and/or altered as they are generated before passing back
 * to the user.
 *
 * Return value:  a new &librdf_stream object or NULL on failure
 **/
librdf_stream*
librdf_new_stream(librdf_world *world, 
                  void* context,
		  int (*is_end_method)(void*),
		  int (*next_method)(void*),
                  void* (*get_method)(void*, int),
		  void (*finished_method)(void*))
{
  librdf_stream* new_stream;
  
  new_stream=(librdf_stream*)LIBRDF_CALLOC(librdf_stream, 1, 
					   sizeof(librdf_stream));
  if(!new_stream)
    return NULL;


  new_stream->context=context;

  new_stream->is_end_method=is_end_method;
  new_stream->next_method=next_method;
  new_stream->get_method=get_method;
  new_stream->finished_method=finished_method;

  new_stream->is_finished=0;
  new_stream->current=NULL;
  
  return new_stream;
}


/**
 * librdf_free_stream - Destructor - destroy an libdf_stream object
 * @stream: &librdf_stream object
 **/
void
librdf_free_stream(librdf_stream* stream) 
{
  stream->finished_method(stream->context);

  if(stream->free_map)
    stream->free_map(stream->map_context);
  
  LIBRDF_FREE(librdf_stream, stream);
}


/*
 * librdf_stream_get_next_mapped_element - helper function to get the next element with map appled
 * @stream: &librdf_stream object
 * 
 * A helper function that gets the next element subject to the user
 * defined map function, if set by librdf_stream_set_map(),
 * 
 * Return value: the next statement or NULL at end of stream
 */
static librdf_statement*
librdf_stream_update_current_statement(librdf_stream* stream)
{
  librdf_statement* statement=NULL;

  if(stream->is_updated)
    return stream->current;
  
  /* find next statement subject to map */
  while(!stream->is_end_method(stream->context)) {
    statement=(librdf_statement*)stream->get_method(stream->context,
                                 LIBRDF_STREAM_GET_METHOD_GET_OBJECT);
    if(!statement)
      break;

    if(!stream->map)
      break;
    
    /* apply the map to the statement  */
    statement=stream->map(stream->map_context, statement);

    /* found something, return it */
    if(statement)
      break;

    stream->next_method(stream->context);
  }

  stream->current=statement;
  if(!stream->current)
    stream->is_finished=1;

  stream->is_updated=1;

  return statement;
}


/**
 * librdf_stream_end - Test if the stream has ended
 * @stream: &librdf_stream object
 * 
 * Return value: non 0 at end of stream.
 **/
int
librdf_stream_end(librdf_stream* stream) 
{
  /* always end of NULL stream */
  if(!stream || stream->is_finished)
    return 1;
  
  librdf_stream_update_current_statement(stream);

  return stream->is_finished;
}


/**
 * librdf_stream_next - Move to the next librdf_statement in the stream
 * @stream: &librdf_stream object
 *
 * Return value: non 0 if the stream has finished
 **/
int
librdf_stream_next(librdf_stream* stream) 
{
  if(!stream || stream->is_finished)
    return 1;

  if(stream->next_method(stream->context)) {
    stream->is_finished=1;
    return 1;
  }
  
  stream->is_updated=0;
  librdf_stream_update_current_statement(stream);

  return stream->is_finished;
}


/**
 * librdf_stream_get_object - Get the current librdf_statement in the stream
 * @stream: &librdf_stream object
 *
 * Return value: the current &librdf_statement object or NULL at end of stream.
 **/
librdf_statement*
librdf_stream_get_object(librdf_stream* stream) 
{
  if(stream->is_finished)
    return NULL;

  return librdf_stream_update_current_statement(stream);
}


/**
 * librdf_stream_get_context - Get the context of the current object on the stream
 * @stream: the &librdf_stream object
 *
 * Return value: The context or NULL if the stream has finished.
 **/
void*
librdf_stream_get_context(librdf_stream* stream) 
{
  if(stream->is_finished)
    return NULL;

  if(!librdf_stream_update_current_statement(stream))
    return NULL;

  return stream->get_method(stream->context, 
                            LIBRDF_STREAM_GET_METHOD_GET_CONTEXT);
}


/**
 * librdf_stream_set_map - Set the filtering/mapping function for the stream
 * @stream: &librdf_stream object
 * @map: mapping function.
 * @free_map: free map context function 
 * @map_context: context
 * 
 * The function 
 * is called with the mapping context and the next statement.  The return
 * value of the mapping function is then passed on to the user, if not NULL.
 * If NULL is returned, that statement is not emitted.
 **/
void
librdf_stream_set_map(librdf_stream* stream, 
		      librdf_statement* (*map)(void* map_context, librdf_statement* statement), 
                      void (*free_context)(void *map_context),
		      void* map_context) 
{
  stream->map=map;
  stream->free_map=free_context;

  stream->map_context=map_context;
}



static int librdf_stream_from_node_iterator_end_of_stream(void* context);
static int librdf_stream_from_node_iterator_next_statement(void* context);
static void* librdf_stream_from_node_iterator_get_statement(void* context, int flags);
static void librdf_stream_from_node_iterator_finished(void* context);

typedef struct {
  librdf_iterator *iterator;
  librdf_statement *current; /* shared statement */
  librdf_statement_part field;
} librdf_stream_from_node_iterator_stream_context;



/**
 * librdf_new_stream_from_node_iterator - Constructor - create a new librdf_stream from an iterator of nodes
 * @iterator: &librdf_iterator of &librdf_node objects
 * @statement: &librdf_statement prototype with one NULL node space
 * @field: node part of statement
 *
 * Creates a new &librdf_stream using the passed in &librdf_iterator
 * which generates a series of &librdf_node objects.  The resulting
 * nodes are then inserted into the given statement and returned.
 * The field attribute indicates which statement node is being generated.
 *
 * Return value: a new &librdf_stream object or NULL on failure
 **/
librdf_stream*
librdf_new_stream_from_node_iterator(librdf_iterator* iterator,
                                     librdf_statement* statement,
                                     librdf_statement_part field)
{
  librdf_stream_from_node_iterator_stream_context *scontext;
  librdf_stream *stream;

  scontext=(librdf_stream_from_node_iterator_stream_context*)LIBRDF_CALLOC(librdf_stream_from_node_iterator_stream_context, 1, sizeof(librdf_stream_from_node_iterator_stream_context));
  if(!scontext)
    return NULL;

  /* copy the prototype statement */
  statement=librdf_new_statement_from_statement(statement);
  if(!statement) {
    LIBRDF_FREE(librdf_stream_from_node_iterator_stream_context, scontext);
    return NULL;
  }

  scontext->iterator=iterator;
  scontext->current=statement;
  scontext->field=field;
  
  stream=librdf_new_stream(iterator->world,
                           (void*)scontext,
                           &librdf_stream_from_node_iterator_end_of_stream,
                           &librdf_stream_from_node_iterator_next_statement,
                           &librdf_stream_from_node_iterator_get_statement,
                           &librdf_stream_from_node_iterator_finished);
  if(!stream) {
    librdf_stream_from_node_iterator_finished((void*)scontext);
    return NULL;
  }
  
  return stream;  
}


static int
librdf_stream_from_node_iterator_end_of_stream(void* context)
{
  librdf_stream_from_node_iterator_stream_context* scontext=(librdf_stream_from_node_iterator_stream_context*)context;

  return librdf_iterator_end(scontext->iterator);
}


static int
librdf_stream_from_node_iterator_next_statement(void* context)
{
  librdf_stream_from_node_iterator_stream_context* scontext=(librdf_stream_from_node_iterator_stream_context*)context;

  return librdf_iterator_next(scontext->iterator);
}


static void*
librdf_stream_from_node_iterator_get_statement(void* context, int flags)
{
  librdf_stream_from_node_iterator_stream_context* scontext=(librdf_stream_from_node_iterator_stream_context*)context;
  librdf_node* node;
  
  switch(flags) {
    case LIBRDF_ITERATOR_GET_METHOD_GET_OBJECT:

      if(!(node=(librdf_node*)librdf_iterator_get_object(scontext->iterator)))
        return NULL;

      /* The node object above is shared, no need to free it before
       * assigning to the statement, which is also shared, and
       * return to the user.
       */
      switch(scontext->field) {
        case LIBRDF_STATEMENT_SUBJECT:
          librdf_statement_set_subject(scontext->current, node);
          break;
        case LIBRDF_STATEMENT_PREDICATE:
          librdf_statement_set_predicate(scontext->current, node);
          break;
        case LIBRDF_STATEMENT_OBJECT:
          librdf_statement_set_object(scontext->current, node);
          break;
        default:
          LIBRDF_ERROR2(scontext->iterator->world, 
                        librdf_stream_from_node_iterator_next_statement,
                        "Illegal statement field %d seen\n", scontext->field);
          return NULL;
      }
      
      return scontext->current;

    case LIBRDF_ITERATOR_GET_METHOD_GET_CONTEXT:
      return (librdf_statement*)librdf_iterator_get_context(scontext->iterator);
    default:
      LIBRDF_ERROR2(scontext->iterator->world,
                    librdf_stream_from_node_iterator_get_statement,
                    "Unknown iterator method flag %d\n", flags);
      return NULL;
  }

}


static void
librdf_stream_from_node_iterator_finished(void* context)
{
  librdf_stream_from_node_iterator_stream_context* scontext=(librdf_stream_from_node_iterator_stream_context*)context;
  
  if(scontext->iterator)
    librdf_free_iterator(scontext->iterator);

  if(scontext->current) {
    switch(scontext->field) {
      case LIBRDF_STATEMENT_SUBJECT:
        librdf_statement_set_subject(scontext->current, NULL);
        break;
      case LIBRDF_STATEMENT_PREDICATE:
        librdf_statement_set_predicate(scontext->current, NULL);
        break;
      case LIBRDF_STATEMENT_OBJECT:
        librdf_statement_set_object(scontext->current, NULL);
        break;
      default:
        LIBRDF_ERROR2(scontext->iterator->world,
                      librdf_stream_from_node_iterator_finished,
                      "Illegal statement field %d seen\n", scontext->field);
    }
    librdf_free_statement(scontext->current);
  }

  LIBRDF_FREE(librdf_stream_from_node_iterator_stream_context, scontext);
}


/**
 * librdf_stream_print - print the stream
 * @stream: the stream object
 * @fh: the FILE stream to print to
 *
 * This prints the remaining statements of the stream to the given
 * file handle.  Note that after this method is called the stream
 * will be empty so that librdf_stream_end() will always be true
 * and librdf_stream_next() will always return NULL.  The only
 * useful operation is to dispose of the stream with the
 * librdf_free_stream() destructor.
 * 
 * This method is for debugging and the format of the output should
 * not be relied on.
 * 
 **/
void
librdf_stream_print(librdf_stream *stream, FILE *fh)
{
  if(!stream)
    return;

  while(!librdf_stream_end(stream)) {
    unsigned char *s;
    librdf_statement* statement=librdf_stream_get_object(stream);
    librdf_node* context_node=(librdf_node*)librdf_stream_get_context(stream);
    if(!statement)
      break;

    s=librdf_statement_to_string(statement);
    if(s) {
      fputs("  ", fh);
      fputs((const char*)s, fh);
      if(context_node) {
        fputs(" with context ", fh);
        librdf_node_print(context_node, fh);
      }
      fputs("\n", fh);
      LIBRDF_FREE(cstring, s);
    }
    librdf_stream_next(stream);
  }
}

#endif


/* TEST CODE */


#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

#define STREAM_NODES_COUNT 6
#define NODE_URI_PREFIX "http://example.org/node"

int
main(int argc, char *argv[]) 
{
  librdf_statement *statement;
  librdf_stream* stream;
  char *program=argv[0];
  librdf_world *world;
  librdf_uri* prefix_uri;
  librdf_node* nodes[STREAM_NODES_COUNT];
  int i;
  librdf_iterator* iterator;
  int count;
  
  world=librdf_new_world();
  librdf_world_init_mutex(world);

  librdf_init_hash(world);
  librdf_init_uri(world);
  librdf_init_node(world);

  prefix_uri=librdf_new_uri(world, (const unsigned char*)NODE_URI_PREFIX);
  if(!prefix_uri) {
    fprintf(stderr, "%s: Failed to create prefix URI\n", program);
    return(1);
  }

  for(i=0; i < STREAM_NODES_COUNT; i++) {
    unsigned char buf[2];
    buf[0]='a'+i;
    buf[1]='\0';
    nodes[i]=librdf_new_node_from_uri_local_name(world, prefix_uri, buf);
    if(!nodes[i]) {
      fprintf(stderr, "%s: Failed to create node %i (%s)\n", program, i, buf);
      return(1);
    }
  }
  
  fprintf(stdout, "%s: Creating static node iterator\n", program);
  iterator=librdf_node_static_iterator_create(nodes, STREAM_NODES_COUNT);
  if(!iterator) {
    fprintf(stderr, "%s: Failed to create static node iterator\n", program);
    return(1);
  }

  statement=librdf_new_statement_from_nodes(world,
                                            librdf_new_node_from_uri_string(world, (const unsigned char*)"http://example.org/resource"),
                                            librdf_new_node_from_uri_string(world, (const unsigned char*)"http://example.org/property"),
                                            NULL);
  if(!statement) {
    fprintf(stderr, "%s: Failed to create statement\n", program);
    return(1);
  }

  fprintf(stdout, "%s: Creating stream from node iterator\n", program);
  stream=librdf_new_stream_from_node_iterator(iterator, statement, LIBRDF_STATEMENT_OBJECT);
  if(!stream) {
    fprintf(stderr, "%s: Failed to createstatic  node stream\n", program);
    return(1);
  }
  

  /* This is to check that the stream_from_node_iterator code
   * *really* takes a copy of what it needs from statement 
   */
  fprintf(stdout, "%s: Freeing statement\n", program);
  librdf_free_statement(statement);


  fprintf(stdout, "%s: Listing static node stream\n", program);
  count=0;
  while(!librdf_stream_end(stream)) {
    librdf_statement* s_statement=librdf_stream_get_object(stream);
    if(!s_statement) {
      fprintf(stderr, "%s: librdf_stream_current returned NULL when not end of stream\n", program);
      return(1);
    }

    fprintf(stdout, "%s: statement %d is: ", program, count);
    librdf_statement_print(s_statement, stdout);
    fputc('\n', stdout);
    
    librdf_stream_next(stream);
    count++;
  }

  if(count != STREAM_NODES_COUNT) {
    fprintf(stderr, "%s: Stream returned %d statements, expected %d\n", program,
            count, STREAM_NODES_COUNT);
    return(1);
  }

  fprintf(stdout, "%s: stream from node iterator worked ok\n", program);


  fprintf(stdout, "%s: Freeing stream\n", program);
  librdf_free_stream(stream);


  fprintf(stdout, "%s: Freeing nodes\n", program);
  for (i=0; i<STREAM_NODES_COUNT; i++) {
    librdf_free_node(nodes[i]);
  }

  librdf_free_uri(prefix_uri);
  
  librdf_finish_node(world);
  librdf_finish_uri(world);
  librdf_finish_hash(world);

  LIBRDF_FREE(librdf_world, world);
  
  /* keep gcc -Wall happy */
  return(0);
}

#endif
