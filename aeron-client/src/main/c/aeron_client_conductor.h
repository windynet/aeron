/*
 * Copyright 2014-2020 Real Logic Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AERON_C_CLIENT_CONDUCTOR_H
#define AERON_C_CLIENT_CONDUCTOR_H

#include "concurrent/aeron_mpsc_concurrent_array_queue.h"
#include "concurrent/aeron_broadcast_receiver.h"
#include "concurrent/aeron_mpsc_rb.h"
#include "command/aeron_control_protocol.h"
#include "aeronc.h"
#include "concurrent/aeron_counters_manager.h"

#define AERON_CLIENT_COMMAND_QUEUE_FAIL_THRESHOLD (10)
#define AERON_CLIENT_COMMAND_RB_FAIL_THRESHOLD (10)

#define AERON_CLIENT_CONDUCTOR_IDLE_SLEEP_NS (16 * 1000 * 1000L)

typedef enum aeron_client_registration_status_en
{
    AERON_CLIENT_AWAITING_MEDIA_DRIVER,
    AERON_CLIENT_REGISTERED_MEDIA_DRIVER,
    AERON_CLIENT_ERRORED_MEDIA_DRIVER
}
aeron_client_registration_status_t;

typedef enum aeron_client_managed_resource_type_en
{
    AERON_CLIENT_TYPE_PUBLICATION,
    AERON_CLIENT_TYPE_EXCLUSIVE_PUBLICATION,
    AERON_CLIENT_TYPE_SUBSCRIPTION,
    AERON_CLIENT_TYPE_IMAGE
}
aeron_client_managed_resource_type_t;

typedef struct aeron_client_command_base_stct
{
    void (*func)(void *clientd, void *command);
    void *item;
}
aeron_client_command_base_t;

typedef struct aeron_client_registering_resource_stct
{
    aeron_client_command_base_t command_base;
    union aeron_client_registering_resource_un
    {
        aeron_publication_t *publication;
        aeron_exclusive_publication_t *exclusive_publication;
        aeron_subscription_t *subscription;
        aeron_counter_t *counter;
    }
    resource;
    aeron_clock_func_t epoch_clock;

    char *error_message;
    char *uri;
    int64_t registration_id;
    long long registration_deadline_ms;
    int32_t error_code;
    int32_t error_message_length;
    int32_t uri_length;
    int32_t stream_id;
    aeron_client_registration_status_t registration_status;
    aeron_client_managed_resource_type_t type;
}
aeron_client_registering_resource_t;

typedef struct aeron_client_registering_resource_entry_stct
{
    aeron_client_registering_resource_t *resource;
}
aeron_client_registering_resource_entry_t;

typedef struct aeron_client_managed_resource_stct
{
    union aeron_client_managed_resource_un
    {
        aeron_publication_t *publication;
        aeron_exclusive_publication_t *exclusive_publication;
        aeron_subscription_t *subscription;
        aeron_image_t *image;
        aeron_counter_t *counter;
    }
    resource;
    aeron_client_managed_resource_type_t type;
    long long time_of_last_state_change_ns;
    int64_t registration_id;
}
aeron_client_managed_resource_t;

typedef struct aeron_client_conductor_stct
{
    aeron_broadcast_receiver_t to_client_buffer;
    aeron_mpsc_rb_t to_driver_buffer;
    aeron_counters_reader_t counters_reader;

    struct lingering_resources_stct
    {
        size_t length;
        size_t capacity;
        aeron_client_managed_resource_t *array;
    }
    lingering_resources;

    struct active_resources_stct
    {
        size_t length;
        size_t capacity;
        aeron_client_managed_resource_t *array;
    }
    active_resources;

    struct registering_resources_stct
    {
        size_t length;
        size_t capacity;
        aeron_client_registering_resource_entry_t *array;
    }
    registering_resources;

    struct heartbeat_timestamp_stct
    {
        int64_t *addr;
        int32_t counter_id;
    }
    heartbeat_timestamp;

    aeron_mpsc_concurrent_array_queue_t *command_queue;

    uint64_t driver_timeout_ms;
    uint64_t driver_timeout_ns;
    uint64_t inter_service_timeout_ns;
    uint64_t keepalive_interval_ns;
    uint64_t resource_linger_duration_ns;

    long long time_of_last_service_ns;
    long long time_of_last_keepalive_ns;

    int64_t client_id;

    aeron_error_handler_t error_handler;
    void *error_handler_clientd;

    aeron_clock_func_t nano_clock;
    aeron_clock_func_t epoch_clock;
    bool invoker_mode;
    bool pre_touch;
    bool is_terminating;
}
aeron_client_conductor_t;

#define AERON_CLIENT_FORMAT_BUFFER(buffer, format, ...) snprintf(buffer, sizeof(buffer) - 1, format, __VA_ARGS__)

int aeron_client_conductor_init(aeron_client_conductor_t *conductor, aeron_context_t *context);
int aeron_client_conductor_do_work(aeron_client_conductor_t *conductor);
void aeron_client_conductor_on_close(aeron_client_conductor_t *conductor);

void aeron_client_conductor_on_cmd_add_publication(void *clientd, void *item);
void aeron_client_conductor_on_cmd_close_publication(void *clientd, void *item);

int aeron_client_conductor_async_add_publication(
    aeron_async_add_publication_t **async, aeron_client_conductor_t *conductor, const char *uri, int32_t stream_id);
int aeron_client_conductor_async_close_publication(
    aeron_client_conductor_t *conductor, aeron_publication_t *publication);

int aeron_client_conductor_async_add_exclusive_publication(
    aeron_async_add_exclusive_publication_t **async, aeron_client_conductor_t *conductor, const char *uri, int32_t stream_id);
int aeron_client_conductor_async_close_exclusive_publication(
    aeron_client_conductor_t *conductor, aeron_exclusive_publication_t *publication);

int aeron_client_conductor_on_error(aeron_client_conductor_t *conductor, aeron_error_response_t *response);
int aeron_client_conductor_on_publication_ready(
    aeron_client_conductor_t *conductor, aeron_publication_buffers_ready_t *response);

inline int aeron_counter_heartbeat_timestamp_find_counter_id_by_registration_id(
    aeron_counters_reader_t *counters_reader, int32_t type_id, int64_t registration_id)
{
    for (size_t i = 0; i < counters_reader->max_counter_id; i++)
    {
        aeron_counter_metadata_descriptor_t *metadata = (aeron_counter_metadata_descriptor_t *)(
            counters_reader->metadata + AERON_COUNTER_METADATA_OFFSET(i));
        int32_t record_state;

        AERON_GET_VOLATILE(record_state, metadata->state);

        if (AERON_COUNTER_RECORD_ALLOCATED == record_state)
        {
            if (type_id == metadata->type_id && registration_id == *(int64_t *)(metadata->key))
            {
                return i;
            }
        }
    }

    return AERON_NULL_COUNTER_ID;
}

inline bool aeron_counter_heartbeat_timestamp_is_active(
    aeron_counters_reader_t *counters_reader, int32_t counter_id, int32_t type_id, int64_t registration_id)
{
    aeron_counter_metadata_descriptor_t *metadata = (aeron_counter_metadata_descriptor_t *)(
        counters_reader->metadata + AERON_COUNTER_METADATA_OFFSET(counter_id));
    int32_t record_state;

    AERON_GET_VOLATILE(record_state, metadata->state);

    return
        AERON_COUNTER_RECORD_ALLOCATED == record_state &&
        type_id == metadata->type_id &&
        registration_id == *(int64_t *)(metadata->key);
}

#endif //AERON_C_CLIENT_CONDUCTOR_H
