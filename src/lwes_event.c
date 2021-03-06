/*======================================================================*
 * Copyright (c) 2008, Yahoo! Inc. All rights reserved.                 *
 *                                                                      *
 * Licensed under the New BSD License (the "License"); you may not use  *
 * this file except in compliance with the License.  Unless required    *
 * by applicable law or agreed to in writing, software distributed      *
 * under the License is distributed on an "AS IS" BASIS, WITHOUT        *
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.     *
 * See the License for the specific language governing permissions and  *
 * limitations under the License. See accompanying LICENSE file.        *
 *======================================================================*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>

#include "lwes_event.h"
#include "lwes_hash.h"
#include "lwes_marshall_functions.h"

/*************************************************************************
  PRIVATE API prototypes, shouldn't be called by a user of the library.
 *************************************************************************/

/* Create the memory for an event attribute */
static struct lwes_event_attribute *
lwes_event_attribute_create
  (LWES_BYTE attrType,
   void *    attrValue);

static int
lwes_event_add
  (struct lwes_event*       event,
   LWES_CONST_SHORT_STRING  attrNameIn,
   LWES_BYTE                attrType,
   void*                    attrValue);

int
lwes_INT_64_from_hex_string
  (const char *buffer,
   LWES_INT_64* an_int64);

int
lwes_U_INT_64_from_hex_string
  (const char *buffer,
   LWES_U_INT_64* a_uint64);

/*************************************************************************
  PUBLIC API
 *************************************************************************/
struct lwes_event *
lwes_event_create_no_name
  (struct lwes_event_type_db *db)
{
  struct lwes_event *event =
     (struct lwes_event *)malloc (sizeof (struct lwes_event));

  if (event == NULL)
    {
      return NULL;
    }

  event->eventName            = NULL;
  event->number_of_attributes = 0;
  event->type_db              = db;
  event->attributes           = lwes_hash_create ();
  if (event->attributes == NULL)
    {
      free(event);
      return NULL;
    }

  return event;
}

/* PUBLIC : Create the memory for an event */
struct lwes_event *
lwes_event_create
  (struct lwes_event_type_db *db,
   LWES_CONST_SHORT_STRING name)
{
  struct lwes_event *event;

  if (name == NULL)
    {
      return NULL;
    }

  event = (struct lwes_event *)malloc (sizeof (struct lwes_event));

  if (event == NULL)
    {
      return NULL;
    }

  event->eventName            = NULL;
  event->number_of_attributes = 0;
  event->type_db              = db;
  event->attributes           = lwes_hash_create ();
  if (event->attributes == NULL)
    {
      free(event);
      return NULL;
    }

  if (lwes_event_set_name (event,name) < 0)
    {
      /* Having problems, bail and return NULL */
      lwes_hash_destroy (event->attributes);
      free (event);
      return NULL;
    }

  return event;
}

struct lwes_event *
lwes_event_create_with_encoding
  (struct lwes_event_type_db *db,
   LWES_CONST_SHORT_STRING name,
   LWES_INT_16 encoding)
{
  struct lwes_event *event;

  if (name == NULL)
    {
      return NULL;
    }

  event = (struct lwes_event *)malloc(sizeof(struct lwes_event));

  if (event == NULL)
    {
      return NULL;
    }

  event->eventName            = NULL;
  event->number_of_attributes = 0;
  event->type_db              = db;
  event->attributes           = lwes_hash_create ();

  if (event->attributes == NULL)
    {
      free (event);
      return NULL;
    }

  if (lwes_event_set_name(event,name) < 0)
    {
      /* Having memory problems, bail */
      lwes_hash_destroy (event->attributes);
      free (event);
      return NULL;
    }

  if (lwes_event_set_encoding (event,encoding) < 0)
    {
      /* problem setting encoding, free up memory and bail */
      lwes_hash_destroy (event->attributes);
      if (event->eventName != NULL)
        {
          free (event->eventName);
        }
      free (event);
      return NULL;
    }

  return event;
}

int
lwes_event_set_name
  (struct lwes_event *event,
   LWES_CONST_SHORT_STRING name)
{
  if (event == NULL || name == NULL || event->eventName != NULL)
    {
      return -1;
    }

  event->eventName =
    (LWES_SHORT_STRING) malloc (sizeof (LWES_CHAR)*(strlen (name)+1));

  if (event->eventName == NULL)
    {
      return -3;
    }

  strcpy (event->eventName,name);

  return 0;
}

int
lwes_event_set_encoding
  (struct lwes_event *event,
   LWES_INT_16 encoding)
{
  LWES_INT_16 tmp_encoding;

  if (event == NULL)
    {
      return -1;
    }

  if ( lwes_event_get_INT_16 (event, LWES_ENCODING, &tmp_encoding) == -1 )
    {
      return lwes_event_set_INT_16 (event, LWES_ENCODING, encoding);
    }
  return -1;
}


int
lwes_event_get_name
  (struct lwes_event *event, LWES_SHORT_STRING *name)
{
  if (event == NULL || name == NULL)
    {
      return -1;
    }
  *name = event->eventName;
  return 0;
}

int
lwes_event_get_number_of_attributes
  (struct lwes_event *event,
   LWES_U_INT_16 *number)
{
  if (event == NULL || number == NULL)
    {
      return -1;
    }
  *number = event->number_of_attributes;
  return 0;
}

int
lwes_event_get_encoding
  (struct lwes_event *event,
   LWES_INT_16 *encoding)
{
  if (event == NULL || encoding == NULL)
    {
      return -1;
    }

  return lwes_event_get_INT_16 (event,
                                (LWES_SHORT_STRING)LWES_ENCODING,
                                encoding);
}

/* PUBLIC : Cleanup the memory for an event */
int
lwes_event_destroy
  (struct lwes_event *event)
{
  struct lwes_event_attribute *tmp = NULL;
  struct lwes_hash_enumeration e;

  if (event == NULL)
    {
      return 0;
    }

  /* free the parts of the event */
  if (event->eventName != NULL)
    {
      free(event->eventName);
    }

  /* clear out the hash */
  if (lwes_hash_keys (event->attributes, &e))
    {
      while (lwes_hash_enumeration_has_more_elements (&e))
        {
          LWES_SHORT_STRING tmpAttrName =
            lwes_hash_enumeration_next_element (&e);
          tmp =
            (struct lwes_event_attribute *)lwes_hash_remove (event->attributes,
                                                             tmpAttrName);
          /* free the attribute name and value*/
          if (tmpAttrName != NULL)
            {
              free(tmpAttrName);
            }
          if (tmp->value != NULL)
            {
              free(tmp->value);
            }
          /* free the attribute itself*/
          if (tmp != NULL)
            {
              free(tmp);
            }
        }
    }

  /* free the now empty hash */
  lwes_hash_destroy (event->attributes);

  /* finally free the event structure */
  free (event);

  return 0;
}

/* PUBLIC : serialize the event and put it into a byte array */
int
lwes_event_to_bytes
  (struct lwes_event *event,
   LWES_BYTE_P bytes,
   size_t num_bytes,
   size_t offset)
{
  struct lwes_event_attribute *tmp;
  struct lwes_event_attribute *encodingAttr;
  size_t tmpOffset = offset;
  struct lwes_hash_enumeration e;
  int ret = 0;

  if (   event == NULL
      || bytes == NULL
      || num_bytes == 0
      || offset >= num_bytes)
    {
      return -1;
    }

  /* start with the event name */
  if (marshall_SHORT_STRING (event->eventName,
                              bytes,
                              num_bytes,
                              &tmpOffset))
    {
      /* then the number of attributes */
      if (marshall_U_INT_16     (event->number_of_attributes,
                                  bytes,
                                  num_bytes,
                                  &tmpOffset))
        {
          /* handle encoding first if it is set */
          encodingAttr =
            (struct lwes_event_attribute *)
              lwes_hash_get (event->attributes,
                             (LWES_SHORT_STRING)LWES_ENCODING);

          if (encodingAttr)
            {
              void* encodingValue = encodingAttr->value;
              LWES_BYTE encodingType = encodingAttr->type;
              if (encodingValue)
                {
                  if (encodingType == LWES_INT_16_TOKEN)
                    {
                      if (marshall_SHORT_STRING
                            ((LWES_SHORT_STRING)LWES_ENCODING,
                             bytes,
                             num_bytes,
                             &tmpOffset) == 0
                          ||
                          marshall_BYTE
                            (encodingType,
                             bytes,
                             num_bytes,
                             &tmpOffset) == 0
                          ||
                          marshall_INT_16
                            (*((LWES_INT_16 *)encodingValue),
                             bytes,
                             num_bytes,
                             &tmpOffset) == 0)
                        {
                          return -2;
                        }
                    }
                  else
                    {
                      return -3;
                    }
                }
              else
                {
                  return -4;
                }
            }

          /* now iterate over all the other values in the hash */
          if (lwes_hash_keys (event->attributes, &e))
            {
              while (lwes_hash_enumeration_has_more_elements (&e) && ret == 0)
                {
                  LWES_SHORT_STRING tmpAttrName =
                    lwes_hash_enumeration_next_element (&e);

                  /* skip encoding as we've dealt with it above */
                  if (! strcmp(tmpAttrName, LWES_ENCODING))
                    {
                      continue;
                    }

                  tmp =
                    (struct lwes_event_attribute *)
                      lwes_hash_get (event->attributes, tmpAttrName);

                  if (marshall_SHORT_STRING (tmpAttrName,
                                             bytes,
                                             num_bytes,
                                             &tmpOffset) == 0)
                    {
                      ret = -5;
                    }
                  else
                    {
                      if (marshall_BYTE (tmp->type,
                                         bytes,
                                         num_bytes,
                                         &tmpOffset) == 0)
                        {
                          ret = -6;
                        }
                      else
                        {
                          if (tmp->type == LWES_U_INT_16_TOKEN)
                            {
                              if (marshall_U_INT_16 (*((LWES_U_INT_16 *)tmp->value),
                                                     bytes,
                                                     num_bytes,
                                                     &tmpOffset) == 0)
                                {
                                  ret = -7;
                                }
                            }
                          else if (tmp->type == LWES_INT_16_TOKEN)
                            {
                              if (marshall_INT_16 (*((LWES_INT_16 *)tmp->value),
                                                   bytes,
                                                   num_bytes,
                                                   &tmpOffset) == 0)
                                {
                                  ret = -8;
                                }
                            }
                          else if (tmp->type == LWES_U_INT_32_TOKEN)
                            {
                              if (marshall_U_INT_32 (*((LWES_U_INT_32 *)tmp->value),
                                                     bytes,
                                                     num_bytes,
                                                     &tmpOffset) == 0)
                                {
                                  ret = -9;
                                }
                            }
                          else if (tmp->type == LWES_INT_32_TOKEN)
                            {
                              if (marshall_INT_32 (*((LWES_INT_32 *)tmp->value),
                                                   bytes,
                                                   num_bytes,
                                                   &tmpOffset) == 0)
                                {
                                  ret = -10;
                                }
                            }
                          else if (tmp->type == LWES_U_INT_64_TOKEN)
                            {
                              if (marshall_U_INT_64 (*((LWES_U_INT_64 *)tmp->value),
                                                     bytes,
                                                     num_bytes,
                                                     &tmpOffset) == 0)
                                {
                                  ret = -11;
                                }
                            }
                          else if (tmp->type == LWES_INT_64_TOKEN)
                            {
                              if (marshall_INT_64 (*((LWES_INT_64 *)tmp->value),
                                                   bytes,
                                                   num_bytes,
                                                   &tmpOffset) == 0)
                                {
                                  ret = -12;
                                }
                            }
                          else if (tmp->type == LWES_BOOLEAN_TOKEN)
                            {
                              if (marshall_BOOLEAN (*((LWES_BOOLEAN *)tmp->value),
                                                    bytes,
                                                    num_bytes,
                                                    &tmpOffset) == 0)
                                {
                                  ret = -13;
                                }
                            }
                          else if (tmp->type == LWES_IP_ADDR_TOKEN)
                            {
                              if (marshall_IP_ADDR (*((LWES_IP_ADDR *)tmp->value),
                                                    bytes,
                                                    num_bytes,
                                                    &tmpOffset) == 0)
                                {
                                  ret = -14;
                                }
                            }
                          else if (tmp->type == LWES_STRING_TOKEN)
                            {
                              if (marshall_LONG_STRING ((LWES_LONG_STRING)tmp->value,
                                                        bytes,
                                                        num_bytes,
                                                        &tmpOffset) == 0)
                                {
                                  ret = -15;
                                }
                            }
                          else
                            {
                              /* should never be reached, but if it does,
                               * there's some sort of  corruption with this
                               * attribute of the event, so skip it */
                            }
                        }
                    }
                }
            }
        }
      else
        {
          ret = -16;
        }
    }
  else
    {
      ret = -17;
    }

  return ((ret < 0) ? ret : (int)(tmpOffset-offset));
}

/* PUBLIC : deserialize the event from a byte array and into an event */
int
lwes_event_from_bytes
  (struct lwes_event *event,
   LWES_BYTE_P bytes,
   size_t num_bytes,
   size_t offset,
   struct lwes_event_deserialize_tmp *dtmp)
{
  int i;
  LWES_U_INT_16 tmp_number_of_attributes = 0;
  size_t tmpOffset = offset;

  LWES_BYTE         tmp_byte;
  LWES_U_INT_16     tmp_uint16;
  LWES_INT_16       tmp_int16;
  LWES_U_INT_32     tmp_uint32;
  LWES_INT_32       tmp_int32;
  LWES_U_INT_64     tmp_uint64;
  LWES_INT_64       tmp_int64;
  LWES_BOOLEAN      tmp_boolean;
  LWES_IP_ADDR      tmp_ip_addr;
  LWES_SHORT_STRING tmp_short_str;
  LWES_LONG_STRING  tmp_long_str;

  if (   event == NULL
      || bytes == NULL
      || num_bytes == 0
      || offset >= num_bytes
      || dtmp == NULL)
    {
      return -1;
    }

  tmp_short_str = dtmp->tmp_string;
  tmp_long_str  = dtmp->tmp_string_long;

  /* unmarshall the event name */
  if (unmarshall_SHORT_STRING (tmp_short_str,
                                (SHORT_STRING_MAX+1),
                                bytes,
                                num_bytes,
                                &tmpOffset))
    {
      /* copies the data out of tmp_short_str */
      if (lwes_event_set_name (event, tmp_short_str) == 0)
        {
          /* unmarshall the number of elements */
          if (unmarshall_U_INT_16 (&tmp_uint16,bytes,num_bytes,&tmpOffset))
            {
              tmp_number_of_attributes = tmp_uint16;

              for (i = 0; i < tmp_number_of_attributes; i++)
                {
                  /* unmarshall the attribute name */
                  if (unmarshall_SHORT_STRING (tmp_short_str,
                                                (SHORT_STRING_MAX+1),
                                                bytes,
                                                num_bytes,
                                                &tmpOffset))
                    {
                      /* unmarshall the type id */
                      if (unmarshall_BYTE         (&tmp_byte,
                                                    bytes,
                                                    num_bytes,
                                                    &tmpOffset))
                        {
                          if (tmp_byte == LWES_U_INT_16_TOKEN)
                            {
                              if (unmarshall_U_INT_16     (&tmp_uint16,
                                                            bytes,
                                                            num_bytes,
                                                            &tmpOffset))
                                {
                                  if (lwes_event_set_U_INT_16 (event,
                                                                tmp_short_str,
                                                                tmp_uint16)
                                       < 0)
                                    {
                                      return -2;
                                    }
                                }
                              else
                                {
                                  return -3;
                                }
                            }
                          else if (tmp_byte == LWES_INT_16_TOKEN)
                            {
                              if (unmarshall_INT_16       (&tmp_int16,
                                                            bytes,
                                                            num_bytes,
                                                            &tmpOffset))
                                {
                                  if (lwes_event_set_INT_16   (event,
                                                                tmp_short_str,
                                                                tmp_int16)
                                       < 0)
                                    {
                                      return -4;
                                    }
                                 }
                              else
                                {
                                  return -5;
                                }
                            }
                          else if (tmp_byte == LWES_U_INT_32_TOKEN)
                            {
                              if (unmarshall_U_INT_32     (&tmp_uint32,
                                                            bytes,
                                                            num_bytes,
                                                            &tmpOffset))
                                {
                                  if (lwes_event_set_U_INT_32 (event,
                                                                tmp_short_str,
                                                                tmp_uint32)
                                       < 0)
                                    {
                                      return -6;
                                    }
                                }
                              else
                                {
                                  return -7;
                                }
                            }
                          else if (tmp_byte == LWES_INT_32_TOKEN)
                            {
                              if (unmarshall_INT_32       (&tmp_int32,
                                                            bytes,
                                                            num_bytes,
                                                            &tmpOffset))
                                {
                                  if (lwes_event_set_INT_32   (event,
                                                                tmp_short_str,
                                                                tmp_int32)
                                       < 0)
                                    {
                                      return -8;
                                    }
                                }
                              else
                                {
                                  return -9;
                                }
                            }
                          else if (tmp_byte == LWES_U_INT_64_TOKEN)
                            {
                              if (unmarshall_U_INT_64     (&tmp_uint64,
                                                            bytes,
                                                            num_bytes,
                                                            &tmpOffset))
                                {
                                  if (lwes_event_set_U_INT_64 (event,
                                                                tmp_short_str,
                                                                tmp_uint64)
                                       < 0)
                                    {
                                      return -10;
                                    }
                                }
                              else
                                {
                                  return -11;
                                }
                            }
                          else if (tmp_byte == LWES_INT_64_TOKEN)
                            {
                              if (unmarshall_INT_64         (&tmp_int64,
                                                              bytes,
                                                              num_bytes,
                                                              &tmpOffset))
                                {
                                  if (lwes_event_set_INT_64     (event,
                                                                  tmp_short_str,
                                                                  tmp_int64)
                                       < 0)
                                    {
                                      return -12;
                                    }
                                }
                              else
                                {
                                  return -13;
                                }
                            }
                          else if (tmp_byte == LWES_BOOLEAN_TOKEN)
                            {
                              if (unmarshall_BOOLEAN        (&tmp_boolean,
                                                              bytes,
                                                              num_bytes,
                                                              &tmpOffset))
                                {
                                  if (lwes_event_set_BOOLEAN    (event,
                                                                  tmp_short_str,
                                                                  tmp_boolean)
                                       < 0)
                                    {
                                      return -14;
                                    }
                                }
                              else
                                {
                                  return -15;
                                }
                            }
                          else if (tmp_byte == LWES_IP_ADDR_TOKEN)
                            {
                              if (unmarshall_IP_ADDR        (&tmp_ip_addr,
                                                              bytes,
                                                              num_bytes,
                                                              &tmpOffset))
                                {
                                  if (lwes_event_set_IP_ADDR (event,
                                                               tmp_short_str,
                                                               tmp_ip_addr)
                                       < 0)
                                    {
                                      return -16;
                                    }
                                }
                              else
                                {
                                  return -17;
                                }
                            }
                          else if (tmp_byte == LWES_STRING_TOKEN)
                            {
                              if (unmarshall_LONG_STRING (tmp_long_str,
                                                           (LONG_STRING_MAX+1),
                                                           bytes,
                                                           num_bytes,
                                                           &tmpOffset))
                                {
                                  if (lwes_event_set_STRING (event,
                                                              tmp_short_str,
                                                              tmp_long_str)
                                       < 0)
                                    {
                                      return -18;
                                    }
                                }
                              else
                                {
                                  return -19;
                                }
                            }
                          else
                            {
                              return -20;
                            }
                        }
                      else
                        {
                          return -21;
                        }
                    }
                  else
                    {
                      return -22;
                    }
                }
            }
          else
            {
              return -23;
            }
        }
      else
        {
          return -24;
        }
     }
   else
     {
       return -25;
     }

  return (int)(tmpOffset-offset);
}

int lwes_event_set_U_INT_16 (struct lwes_event *       event,
                             LWES_CONST_SHORT_STRING   attrName,
                             LWES_U_INT_16             value)
{
  int ret = 0;
  LWES_U_INT_16 *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_U_INT_16 *)malloc (sizeof (LWES_U_INT_16));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_U_INT_16_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_INT_16 (struct lwes_event *       event,
                           LWES_CONST_SHORT_STRING   attrName,
                           LWES_INT_16               value)
{
  int ret = 0;
  LWES_INT_16 *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_INT_16 *)malloc (sizeof (LWES_INT_16));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_INT_16_TOKEN, attrValue);
  if (ret < 0)
    {
      free(attrValue);
    }
  return ret;
}

int lwes_event_set_U_INT_32 (struct lwes_event *       event,
                             LWES_CONST_SHORT_STRING   attrName,
                             LWES_U_INT_32             value)
{
  int ret = 0;
  LWES_U_INT_32 *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_U_INT_32 *)malloc(sizeof(LWES_U_INT_32));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_U_INT_32_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_INT_32 (struct lwes_event *       event,
                           LWES_CONST_SHORT_STRING   attrName,
                           LWES_INT_32               value)
{
  int ret = 0;
  LWES_INT_32 *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_INT_32 *)malloc (sizeof (LWES_INT_32));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_INT_32_TOKEN, attrValue);
  if (ret < 0)
    {
      free(attrValue);
    }
  return ret;
}

int lwes_event_set_U_INT_64 (struct lwes_event *       event,
                             LWES_CONST_SHORT_STRING   attrName,
                             LWES_U_INT_64             value)
{
  int ret = 0;
  LWES_U_INT_64 *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_U_INT_64 *)malloc (sizeof (LWES_U_INT_64));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_U_INT_64_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_U_INT_64_w_string (struct lwes_event *     event,
                                      LWES_CONST_SHORT_STRING attrName,
                                      LWES_CONST_SHORT_STRING uint64_string)
{
  LWES_INT_64 u_int_64;
  if (event == NULL || attrName == NULL || uint64_string == NULL)
    {
      return -1;
    }
  if (lwes_INT_64_from_hex_string (uint64_string, &u_int_64) < 0)
    {
      return -2;
    }
  return lwes_event_set_U_INT_64 (event, attrName, u_int_64);
}

int lwes_event_set_INT_64 (struct lwes_event *       event,
                           LWES_CONST_SHORT_STRING   attrName,
                           LWES_INT_64               value)
{
  int ret = 0;
  LWES_INT_64 *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_INT_64 *)malloc (sizeof (LWES_INT_64));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_INT_64_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_INT_64_w_string (struct lwes_event       *event,
                                    LWES_CONST_SHORT_STRING  attrName,
                                    LWES_CONST_SHORT_STRING  int64_string)
{
  LWES_INT_64 int_64;

  if (event == NULL || attrName == NULL || int64_string == NULL)
    {
      return -1;
    }

  if (lwes_INT_64_from_hex_string (int64_string,&int_64) < 0)
    {
      return -2;
    }
  return lwes_event_set_INT_64 (event, attrName, int_64);
}

int lwes_event_set_STRING (struct lwes_event *       event,
                           LWES_CONST_SHORT_STRING   attrName,
                           LWES_CONST_LONG_STRING    value)
{
  int ret = 0;
  LWES_LONG_STRING attrValue;

  if (event == NULL || attrName == NULL || value == NULL)
    {
      return -1;
    }

  attrValue = (LWES_LONG_STRING)malloc (sizeof (LWES_CHAR)*(strlen (value)+1));
  if (attrValue == NULL)
    {
      return -3;
    }
  strcpy(attrValue,value);

  ret = lwes_event_add (event, attrName, LWES_STRING_TOKEN, (void*)attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_IP_ADDR (struct lwes_event *       event,
                            LWES_CONST_SHORT_STRING   attrName,
                            LWES_IP_ADDR              value)
{
  int ret = 0;
  LWES_IP_ADDR *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_IP_ADDR *)malloc (sizeof (LWES_IP_ADDR));

  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_IP_ADDR_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_IP_ADDR_w_string (struct lwes_event *       event,
                                     LWES_CONST_SHORT_STRING   attrName,
                                     LWES_CONST_SHORT_STRING  value)
{
  int ret = 0;
  struct in_addr *attrValue;

  if (event == NULL || attrName == NULL || value == NULL)
    {
      return -1;
    }

  attrValue = (LWES_IP_ADDR *)malloc (sizeof (LWES_IP_ADDR));
  if (attrValue == NULL)
    {
      return -3;
    }
  attrValue->s_addr = inet_addr (value);

  ret = lwes_event_add (event, attrName, LWES_IP_ADDR_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}

int lwes_event_set_BOOLEAN (struct lwes_event       * event,
                            LWES_CONST_SHORT_STRING   attrName,
                            LWES_BOOLEAN              value)
{
  int ret = 0;
  LWES_BOOLEAN *attrValue;

  if (event == NULL || attrName == NULL)
    {
      return -1;
    }

  attrValue = (LWES_BOOLEAN *)malloc (sizeof (LWES_BOOLEAN));
  if (attrValue == NULL)
    {
      return -3;
    }
  *attrValue = value;

  ret = lwes_event_add (event, attrName, LWES_BOOLEAN_TOKEN, attrValue);
  if (ret < 0)
    {
      free (attrValue);
    }
  return ret;
}


/* ACCESSOR METHODS */

int lwes_event_get_U_INT_16 (struct lwes_event       *event,
                             LWES_CONST_SHORT_STRING  name,
                             LWES_U_INT_16           *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_U_INT_16 *)tmp->value));
      return 0;
    }

  return -1;
}

int lwes_event_get_INT_16 (struct lwes_event       *event,
                           LWES_CONST_SHORT_STRING  name,
                           LWES_INT_16             *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_INT_16 *)tmp->value));
      return 0;
    }
  return -1;
}

int lwes_event_get_U_INT_32 (struct lwes_event       *event,
                             LWES_CONST_SHORT_STRING  name,
                             LWES_U_INT_32           *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_U_INT_32 *)tmp->value));
      return 0;
    }

  return -1;
}

int lwes_event_get_INT_32 (struct lwes_event       *event,
                           LWES_CONST_SHORT_STRING  name,
                           LWES_INT_32             *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_INT_32 *)tmp->value));
      return 0;
    }

  return -1;
}

int lwes_event_get_U_INT_64 (struct lwes_event       *event,
                             LWES_CONST_SHORT_STRING  name,
                             LWES_U_INT_64           *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_U_INT_64 *)tmp->value));
      return 0;
    }

  return -1;
}

int lwes_event_get_INT_64 (struct lwes_event       *event,
                           LWES_CONST_SHORT_STRING  name,
                           LWES_INT_64             *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_INT_64 *)tmp->value));
      return 0;
    }

  return -1;
}

int lwes_event_get_STRING (struct lwes_event       *event,
                           LWES_CONST_SHORT_STRING  name,
                           LWES_LONG_STRING        *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (((LWES_LONG_STRING)tmp->value));
      return 0;
    }

  return -1;
}
int lwes_event_get_IP_ADDR (struct lwes_event       *event,
                            LWES_CONST_SHORT_STRING  name,
                            LWES_IP_ADDR            *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get (event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_IP_ADDR *)tmp->value));
      return 0;
    }
  return -1;
}

int lwes_event_get_BOOLEAN (struct lwes_event       *event,
                            LWES_CONST_SHORT_STRING  name,
                            LWES_BOOLEAN            *value)
{
  struct lwes_event_attribute *tmp;

  if (event == NULL || name == NULL || value == NULL)
    {
      return -1;
    }

  tmp = (struct lwes_event_attribute *)lwes_hash_get(event->attributes, name);

  if (tmp)
    {
      *value = (*((LWES_BOOLEAN *)tmp->value));
      return 0;
    }
  return -1;
}

/*************************************************************************
  PRIVATE API
 *************************************************************************/
/* Create the memory for an attribute */
static struct lwes_event_attribute *
lwes_event_attribute_create (LWES_BYTE attrType, void *attrValue)
{
  struct lwes_event_attribute *attribute =
    (struct lwes_event_attribute *)
      malloc (sizeof (struct lwes_event_attribute));

  if (attribute == NULL)
    {
      return NULL;
    }

  attribute->type  = attrType;
  attribute->value = attrValue;

  return attribute;
}

/* add an attribute to an event */
static int
lwes_event_add (struct lwes_event*       event,
                LWES_CONST_SHORT_STRING  attrNameIn,
                LWES_BYTE                attrType,
                void*                    attrValue)
{
  struct lwes_event_attribute* attribute = NULL;
  LWES_SHORT_STRING            attrName  = NULL;
  int ret                                = 0;

  /* check against the event db */
  if (event->type_db != NULL
       && lwes_event_type_db_check_for_attribute (event->type_db,
                                                  attrNameIn,
                                                  event->eventName) == 0)
    {
      return -1;
    }
  if (event->type_db != NULL
       && lwes_event_type_db_check_for_type (event->type_db,
                                             attrType,
                                             attrNameIn,
                                             event->eventName) == 0)
    {
      return -2;
    }

  /* copy the attribute name */
  attrName =
      (LWES_SHORT_STRING) malloc( sizeof(LWES_CHAR)*(strlen (attrNameIn)+1));
  if (attrName == NULL)
    {
      return -3;
    }
  attrName[0] = '\0';
  strcat (attrName,attrNameIn);

  /* create the attribute */
  attribute = lwes_event_attribute_create (attrType, attrValue);
  if (attribute == NULL)
    {
      free (attrName);
      return -3;
    }

  /* Try and put something into the hash */
  ret = lwes_hash_put (event->attributes, attrName, attribute);

  /* return code greater than or equal to 0 is okay, so increment the
   * number_of_attributes, otherwise, send out the failure value
   */
  if (ret < 0)
    {
      free (attribute);
      free (attrName);
      return ret;
    }
  event->number_of_attributes++;

  return event->number_of_attributes;
}

int
lwes_U_INT_64_from_hex_string
  (const char *buffer,
   LWES_U_INT_64* a_uint64)
{
  char *endptr;

  /* manpage for strtoull suggests setting errno to zero then checking
   * for error afterwards
   */
  errno = 0;
  *a_uint64 = strtoull (buffer, &endptr, 16);

  /* it's considered an error if it overflows (ie, errno is ERANGE)
   * or if the entire string is not used (ie, in_length is greater than
   * out_length)
   */
  if (errno == ERANGE
      || ((int)strlen (buffer) > (int)(endptr - buffer)))
    {
      return -1;
    }

  return 0;
}

int
lwes_INT_64_from_hex_string
  (const char *buffer,
   LWES_INT_64* an_int64)
{
  /*
   * totally bogus, but on freebsd strtoll for hex can not reach the
   * maximum hex value of 0xffffffffffffffff, so instead I use
   * strtoull which does reach the upper range, and then cast
   */
  LWES_U_INT_64 a_uint64;
  int ret;

  ret = lwes_U_INT_64_from_hex_string (buffer, &a_uint64);

  if (ret < 0)
    {
      return ret;
    }

  *an_int64 = (LWES_INT_64)a_uint64;

  return 0;
}

int
lwes_event_keys
  (struct lwes_event * event,
   struct lwes_event_enumeration *enumeration)
{
  return lwes_hash_keys (event->attributes, &(enumeration->hash_enum));
}

int
lwes_event_enumeration_next_element
  (struct lwes_event_enumeration *enumeration,
   LWES_CONST_SHORT_STRING *key,
   LWES_TYPE *type)
{
  LWES_SHORT_STRING tmpAttrName =
     lwes_hash_enumeration_next_element (&(enumeration->hash_enum));
  (*key) = NULL;
  (*type) = LWES_TYPE_UNDEFINED;
  
  if ( tmpAttrName != NULL)
    {
      struct lwes_event_attribute *tmp =
         (struct lwes_event_attribute *)
           lwes_hash_get (enumeration->hash_enum.enum_hash, (tmpAttrName));
      if (tmp != NULL)
        {
          (*type) = (LWES_TYPE) (tmp->type);
          (*key) = tmpAttrName;
          return 1;
        }
    }
  return 0;
}


