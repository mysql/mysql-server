/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "gcs_message.h"

Gcs_message::Gcs_message(Gcs_member_identifier origin,
                         Gcs_group_identifier destination,
                         gcs_message_delivery_guarantee guarantee)
{
  this->origin= new Gcs_member_identifier(*origin.get_member_id());
  this->destination= new Gcs_group_identifier(destination.get_group_id());

  this->guarantee= guarantee;

  this->header= new vector<uchar>();
  this->payload= new vector<uchar>();
}

Gcs_message::~Gcs_message()
{
  delete this->header;
  delete this->payload;

  delete this->destination;
  delete this->origin;
}

uchar* Gcs_message::get_header()
{
  return &this->header->front();
}

size_t Gcs_message::get_header_length()
{
  return this->header->size();
}

uchar* Gcs_message::get_payload()
{
  return &this->payload->front();
}

size_t Gcs_message::get_payload_length()
{
  return this->payload->size();
}

Gcs_member_identifier* Gcs_message::get_origin()
{
  return origin;
}

Gcs_group_identifier* Gcs_message::get_destination()
{
  return destination;
}

void Gcs_message::append_to_header(uchar* to_append, size_t to_append_len)
{
  this->header->insert(this->header->end(),
                       to_append,
                       to_append+to_append_len);
}

void Gcs_message::append_to_payload(uchar* to_append, size_t to_append_len)
{
  this->payload->insert(this->payload->end(),
                        to_append,
                        to_append+to_append_len);
}

gcs_message_delivery_guarantee Gcs_message::get_delivery_guarantee()
{
  return guarantee;
}

vector<uchar>* Gcs_message::encode()
{
  vector<uchar>* encoded_message = new vector<uchar>();

  uchar guarantee_buffer[GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE];
  gcs_message_delivery_guarantee* guarantee_ptr= &(this->guarantee);
  memcpy(&guarantee_buffer, guarantee_ptr,
         GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE);

  encoded_message
          ->insert(encoded_message->end(),
                   guarantee_buffer,
                   guarantee_buffer+GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE);

  uchar size_t_buffer[GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH];

  size_t header_length= get_header_length();

  memcpy(&size_t_buffer, &header_length, GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH);

  encoded_message->insert(encoded_message->end(),
                          size_t_buffer,
                          size_t_buffer+GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH);

  encoded_message->insert(encoded_message->end(),
                          this->header->begin(),
                          this->header->end());

  size_t payload_length= get_payload_length();

  memcpy(size_t_buffer, &payload_length, GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH);

  encoded_message->insert(encoded_message->end(),
                          size_t_buffer,
                          size_t_buffer+GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH);

  encoded_message->insert(encoded_message->end(),
                          this->payload->begin(),
                          this->payload->end());

  return encoded_message;
}

void Gcs_message::decode(uchar* data, size_t data_len)
{
  //point to beginning which is header len
  uchar* slider= data;

  memcpy(&guarantee, slider, GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE);

  slider+= GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE;

  size_t header_len= 0;
  memcpy(&header_len, slider, GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH);

  //slide to header data
  slider+= GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH;
  append_to_header(slider, header_len);

  //slide to payload size
  slider+= header_len;

  size_t payload_len= 0;
  memcpy(&payload_len, slider, GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH);

  //slide to payload
  slider+= GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH;
  append_to_payload(slider, payload_len);
}
