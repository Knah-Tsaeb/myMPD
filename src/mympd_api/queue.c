/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "mpd/queue.h"
#include "src/mympd_api/queue.h"

#include "src/lib/jsonrpc.h"
#include "src/lib/log.h"
#include "src/lib/sds_extras.h"
#include "src/lib/utility.h"
#include "src/mpd_client/errorhandler.h"
#include "src/mpd_client/shortcuts.h"
#include "src/mpd_client/tags.h"
#include "src/mympd_api/status.h"
#include "src/mympd_api/sticker.h"
#include "src/mympd_api/webradios.h"

#include <string.h>

/**
 * Private definitions
 */
sds print_queue_entry(struct t_partition_state *partition_state, sds buffer, const struct t_tags *tagcols, struct mpd_song *song);

/**
 * Public functions
 */

/**
 * Removes songs defined by id from the queue
 * @param partition_state pointer to partition state
 * @param song_ids list of song_ids to remove
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_rm_song_ids(struct t_partition_state *partition_state, struct t_list *song_ids, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current;
        while ((current = list_shift_first(song_ids)) != NULL) {
            bool rc = mpd_send_delete_id(partition_state->conn, (unsigned)current->value_i);
            list_node_free(current);
            if (rc == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_delete_id");
                break;
            }
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_delete_id");
}

/**
 * Appends uris to the queue
 * @param partition_state pointer to partition state
 * @param uris uris to append
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_append(struct t_partition_state *partition_state, struct t_list *uris, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current;
        while ((current = list_shift_first(uris)) != NULL) {
            bool rc = mpd_send_add(partition_state->conn, current->key);
            list_node_free(current);
            if (rc == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_add");
                break;
            }
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_add");
}

/**
 * Insert uris into the queue
 * @param partition_state pointer to partition state
 * @param uris uris to insert
 * @param to position to insert
 * @param whence how to interpret the to parameter
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_insert(struct t_partition_state *partition_state, struct t_list *uris, unsigned to, unsigned whence, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current;
        while ((current = list_shift_first(uris)) != NULL) {
            bool rc = mpd_send_add_whence(partition_state->conn, current->key, to, whence);
            list_node_free(current);
            if (rc == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_add");
                break;
            }
            to++;
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_add_whence");
}

/**
 * Replaces the queue with uris
 * @param partition_state pointer to partition state
 * @param uris uris to add
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_replace(struct t_partition_state *partition_state, struct t_list *uris, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        if (mpd_send_clear(partition_state->conn) == false) {
            mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_clear");
        }
        else {
            struct t_list_node *current;
            while ((current = list_shift_first(uris)) != NULL) {
                bool rc = mpd_send_add(partition_state->conn, current->key);
                list_node_free(current);
                if (rc == false) {
                    mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_add");
                    break;
                }
            }
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_add");
}

/**
 * Plays the newest inserted song in the queue
 * @param partition_state pointer to partition state
 * @return true on success, else false
 */
bool mympd_api_queue_play_newly_inserted(struct t_partition_state *partition_state) {
    unsigned song_pos = 0;
    unsigned song_id = 0;
    if (mpd_send_queue_changes_brief(partition_state->conn, partition_state->queue_version)) {
        mpd_recv_queue_change_brief(partition_state->conn, &song_pos, &song_id);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover(partition_state, NULL, "mpd_send_queue_changes_brief") == false) {
        return false;
    }
    mpd_run_play_id(partition_state->conn, song_id);
    return mympd_check_error_and_recover(partition_state, NULL, "mpd_run_play_id");
}

/**
 * Sets the priority of entries in the queue.
 * The priority has only an effect in random mode.
 * @param partition_state pointer to partition state
 * @param song_ids song ids in the queue
 * @param priority priority to set, max 255
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_prio_set(struct t_partition_state *partition_state, struct t_list *song_ids, unsigned priority, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current;
        while ((current = list_shift_first(song_ids)) != NULL) {
            bool rc = mpd_send_prio_id(partition_state->conn, (unsigned)current->value_i, priority);
            list_node_free(current);
            if (rc == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_prio_id");
                break;
            }
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_prio_id");
}

/**
 * Sets the priority to the highest value of a song in the queue.
 * The priority has only an effect in random mode.
 * @param partition_state pointer to partition state
 * @param song_ids song ids in the queue
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_prio_set_highest(struct t_partition_state *partition_state, struct t_list *song_ids, sds *error) {
    //default prio is 0
    unsigned priority = 1;
    int next_song_id = -1;
    //try to get prio of next song
    struct mpd_status *status = mpd_run_status(partition_state->conn);
    if (status != NULL) {
        next_song_id = mpd_status_get_next_song_id(status);
        mpd_status_free(status);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover(partition_state, NULL, "mpd_run_status") == false) {
        return false;
    }
    
    if (next_song_id > -1 ) {
        bool rc = mpd_send_get_queue_song_id(partition_state->conn, (unsigned)next_song_id);
        if (rc == true) {
            struct mpd_song *song = mpd_recv_song(partition_state->conn);
            if (song != NULL) {
                priority = mpd_song_get_prio(song);
                priority++;
                mpd_song_free(song);
            }
        }
        mpd_response_finish(partition_state->conn);
        if (mympd_check_error_and_recover(partition_state, NULL, "mpd_send_get_queue_song_id") == false) {
            return false;
        }
    }
    if (priority > MPD_QUEUE_PRIO_MAX) {
        MYMPD_LOG_WARN(partition_state->name, "MPD queue priority limit reached, setting it to max %d", MPD_QUEUE_PRIO_MAX);
        priority = MPD_QUEUE_PRIO_MAX;
    }
    return mympd_api_queue_prio_set(partition_state, song_ids, priority, error);
}

/**
 * Appends playlists to the queue
 * @param partition_state pointer to partition state
 * @param plists playlists to append
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_append_plist(struct t_partition_state *partition_state, struct t_list *plists, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current = plists->head;
        while (current != NULL) {
            current->key = resolv_mympd_uri(current->key, partition_state->mpd_state->mpd_host, partition_state->mympd_state->config);
            if (mpd_send_load(partition_state->conn, current->key) == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_load");
                break;
            }
            current = current->next;
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_load");
}

/**
 * Moves song ids to relative position after current song
 * @param partition_state pointer to partition state
 * @param song_ids song ids to move
 * @param to relative position
 * @param whence how to interpret the to parameter
 * @param error pointer to an already allocated sds string for the error message
 * @return bool true on success, else false
 */
bool mympd_api_queue_move_relative(struct t_partition_state *partition_state, struct t_list *song_ids, unsigned to, unsigned whence, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current = song_ids->head;
        while (current != NULL) {
            if (mpd_send_move_id_whence(partition_state->conn, (unsigned)current->value_i, to, whence) == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_move_id_whence");
                break;
            }
            current = current->next;
            to++;
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_move_id_whence");
}

/**
 * Insert playlists into the queue
 * @param partition_state pointer to partition state
 * @param plists playlists to insert
 * @param to position to insert
 * @param whence how to interpret the to parameter
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_insert_plist(struct t_partition_state *partition_state, struct t_list *plists, unsigned to, unsigned whence, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current = plists->head;
        while (current != NULL) {
            current->key = resolv_mympd_uri(current->key, partition_state->mpd_state->mpd_host, partition_state->mympd_state->config);
            if (mpd_send_load_range_to(partition_state->conn, current->key, 0, UINT_MAX, to, whence) == false) {
                mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_load_range_to");
                break;
            }
            current = current->next;
            to++;
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_load_range_to");
}

/**
 * Replaces the queue with playlists
 * @param partition_state pointer to partition state
 * @param plists playlists to add
 * @param error pointer to an already allocated sds string for the error message
 * @return true on success, else false
 */
bool mympd_api_queue_replace_plist(struct t_partition_state *partition_state, struct t_list *plists, sds *error) {
    if (mpd_command_list_begin(partition_state->conn, false)) {
        if (mpd_send_clear(partition_state->conn) == false) {
            mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_clear");
        }
        else {
            struct t_list_node *current = plists->head;
            while (current != NULL) {
                current->key = resolv_mympd_uri(current->key, partition_state->mpd_state->mpd_host, partition_state->mympd_state->config);
                if (mpd_send_load(partition_state->conn, current->key) == false) {
                    mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_load");
                    break;
                }
                current = current->next;
            }
        }
        mpd_client_command_list_end_check(partition_state);
    }
    mpd_response_finish(partition_state->conn);
    return mympd_check_error_and_recover(partition_state, error, "mpd_send_load");
}

/**
 * Prints the queue status and updates internal state
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @return pointer to buffer
 */
sds mympd_api_queue_status(struct t_partition_state *partition_state, sds buffer) {
    struct mpd_status *status = mpd_run_status(partition_state->conn);
    if (status != NULL) {
        partition_state->queue_version = mpd_status_get_queue_version(status);
        partition_state->queue_length = (long long)mpd_status_get_queue_length(status);
        partition_state->crossfade = (time_t)mpd_status_get_crossfade(status);
        partition_state->play_state = mpd_status_get_state(status);

        if (buffer != NULL) {
            buffer = jsonrpc_notify_start(buffer, JSONRPC_EVENT_UPDATE_QUEUE);
            buffer = mympd_api_status_print(partition_state, buffer, status);
            buffer = jsonrpc_end(buffer);
        }
        mpd_status_free(status);
    }
    mpd_response_finish(partition_state->conn);
    mympd_check_error_and_recover(partition_state, NULL, "mpd_run_status");
    return buffer;
}

/**
 * Crops (removes all but playing song) or clears the queue if no song is playing
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param cmd_id jsonrpc method
 * @param request_id jsonrpc id
 * @param or_clear if true: clears the queue if there is no current playing or paused song
 * @return pointer to buffer
 */
sds mympd_api_queue_crop(struct t_partition_state *partition_state, sds buffer, enum mympd_cmd_ids cmd_id, long request_id, bool or_clear) {
    struct mpd_status *status = mpd_run_status(partition_state->conn);
    unsigned length = 0;
    int playing_song_pos = 0;
    if (status != NULL) {
        length = mpd_status_get_queue_length(status) - 1;
        playing_song_pos = mpd_status_get_song_pos(status);
        mpd_status_free(status);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_run_status") == false) {
        return buffer;
    }

    if (playing_song_pos > -1 &&
        length > 1)
    {
        //there is a current song, crop the queue
        if (mpd_command_list_begin(partition_state->conn, false)) {
            //remove all songs behind current song
            unsigned pos_after = (unsigned)playing_song_pos + 1;
            if (pos_after < length) {
                if (mpd_send_delete_range(partition_state->conn, pos_after, UINT_MAX) == false) {
                    mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_delete_range");
                }
            }
            //remove all songs before current song
            if (playing_song_pos > 0) {
                if (mpd_send_delete_range(partition_state->conn, 0, (unsigned)playing_song_pos) == false) {
                    mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_delete_range");
                }
            }
            mpd_client_command_list_end_check(partition_state);
        }
        mpd_response_finish(partition_state->conn);
        if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_send_delete_range") == false) {
            return buffer;
        }
        buffer = jsonrpc_respond_ok(buffer, cmd_id, request_id, JSONRPC_FACILITY_QUEUE);
    }
    else if (or_clear == true) {
        //no current song
        mpd_run_clear(partition_state->conn);
        if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_run_clear") == true) {
            buffer = jsonrpc_respond_ok(buffer, cmd_id, request_id, JSONRPC_FACILITY_QUEUE);
        }
    }
    else {
        //queue can not be cropped
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
            JSONRPC_FACILITY_QUEUE, JSONRPC_SEVERITY_ERROR, "Can not crop the queue");
        MYMPD_LOG_ERROR(partition_state->name, "Can not crop the queue");
    }

    return buffer;
}

/**
 * Lists the queue
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc id
 * @param offset offset for the list
 * @param limit maximum entries to print
 * @param tagcols columns to print
 * @return pointer to buffer
 */
sds mympd_api_queue_list(struct t_partition_state *partition_state, sds buffer, long request_id,
        long offset, long limit, const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_QUEUE_LIST;
    //we need first the queue status
    struct mpd_status *status = mpd_run_status(partition_state->conn);
    if (status != NULL) {
        partition_state->queue_version = mpd_status_get_queue_version(status);
        partition_state->queue_length = (long long)mpd_status_get_queue_length(status);
        mpd_status_free(status);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_run_status") == false) {
        return buffer;
    }

    //Check offset
    if (offset >= partition_state->queue_length) {
        offset = 0;
    }
    //list the queue
    long real_limit = offset + limit;
    if (mpd_send_list_queue_range_meta(partition_state->conn, (unsigned)offset, (unsigned)real_limit) == true) {
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        buffer = sdscat(buffer, "\"data\":[");
        unsigned total_time = 0;
        long entities_returned = 0;
        struct mpd_song *song;
        while ((song = mpd_recv_song(partition_state->conn)) != NULL) {
            if (entities_returned++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            buffer = print_queue_entry(partition_state, buffer, tagcols, song);
            total_time += mpd_song_get_duration(song);
            mpd_song_free(song);
        }

        buffer = sdscatlen(buffer, "],", 2);
        buffer = tojson_uint(buffer, "totalTime", total_time, true);
        buffer = tojson_llong(buffer, "totalEntities", partition_state->queue_length, true);
        buffer = tojson_long(buffer, "offset", offset, true);
        buffer = tojson_long(buffer, "returnedEntities", entities_returned, false);
        buffer = jsonrpc_end(buffer);
    }
    mpd_response_finish(partition_state->conn);
    mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_send_list_queue_range_meta");
    return buffer;
}

/**
 * Searches the queue
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc id
 * @param tag tag to search
 * @param offset offset for the list
 * @param limit maximum entries to print
 * @param searchstr string to search in tag
 * @param tagcols columns to print
 * @return pointer to buffer
 */
sds mympd_api_queue_search(struct t_partition_state *partition_state, sds buffer, long request_id,
        const char *tag, long offset, long limit, const char *searchstr, const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_QUEUE_SEARCH;
    if (mpd_search_queue_songs(partition_state->conn, false) == false) {
        mpd_search_cancel(partition_state->conn);
        return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
            JSONRPC_SEVERITY_ERROR, "Error creating MPD search queue command");
    }
    enum mpd_tag_type tag_type = mpd_tag_name_parse(tag);
    if (tag_type != MPD_TAG_UNKNOWN) {
        if (mpd_search_add_tag_constraint(partition_state->conn, MPD_OPERATOR_DEFAULT, tag_type, searchstr) == false) {
            mpd_search_cancel(partition_state->conn);
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD search queue command");
        }
    }
    else {
        if (mpd_search_add_any_tag_constraint(partition_state->conn, MPD_OPERATOR_DEFAULT, searchstr) == false) {
            mpd_search_cancel(partition_state->conn);
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD search queue command");
        }
    }

    if (mpd_search_commit(partition_state->conn)) {
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        buffer = sdscat(buffer, "\"data\":[");
        struct mpd_song *song;
        unsigned total_time = 0;
        long entity_count = 0;
        long entities_returned = 0;
        long real_limit = offset + limit;
        while ((song = mpd_recv_song(partition_state->conn)) != NULL) {
            if (entity_count >= offset && entity_count < real_limit) {
                if (entities_returned++) {
                    buffer= sdscatlen(buffer, ",", 1);
                }
                buffer = print_queue_entry(partition_state, buffer, tagcols, song);
                total_time += mpd_song_get_duration(song);
            }
            mpd_song_free(song);
            entity_count++;
        }

        buffer = sdscatlen(buffer, "],", 2);
        buffer = tojson_uint(buffer, "totalTime", total_time, true);
        buffer = tojson_long(buffer, "totalEntities", entity_count, true);
        buffer = tojson_long(buffer, "offset", offset, true);
        buffer = tojson_long(buffer, "returnedEntities", entities_returned, false);
        buffer = jsonrpc_end(buffer);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_search_queue_songs") == false) {
        return buffer;
    }

    return buffer;
}

/**
 * Searches the queue - advanced mode for newer mpd versions
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc id
 * @param expression mpd filter expression
 * @param sort tag to sort
 * @param sortdesc false = ascending, true = descending sort
 * @param offset offset for the list
 * @param limit maximum entries to print
 * @param tagcols columns to print
 * @return pointer to buffer
 */
sds mympd_api_queue_search_adv(struct t_partition_state *partition_state, sds buffer, long request_id,
        sds expression, sds sort, bool sortdesc, unsigned offset, unsigned limit,
        const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_QUEUE_SEARCH_ADV;
    if (mpd_search_queue_songs(partition_state->conn, false) == false) {
        mpd_search_cancel(partition_state->conn);
        return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
            JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
    }

    if (sdslen(expression) == 0) {
        //search requires an expression
        if (mpd_search_add_expression(partition_state->conn, "(base '')") == false) {
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
        }
    }
    else {
        if (mpd_search_add_expression(partition_state->conn, expression) == false) {
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
        }
    }

    enum mpd_tag_type sort_tag = mpd_tag_name_parse(sort);
    if (sort_tag != MPD_TAG_UNKNOWN) {
        sort_tag = get_sort_tag(sort_tag, &partition_state->mpd_state->tags_mpd);
        if (mpd_search_add_sort_tag(partition_state->conn, sort_tag, sortdesc) == false) {
            mpd_search_cancel(partition_state->conn);
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
        }
    }
    else if (strcmp(sort, "LastModified") == 0) {
        //swap order
        sortdesc = sortdesc == false ? true : false;
        if (mpd_search_add_sort_name(partition_state->conn, "Last-Modified", sortdesc) == false) {
            mpd_search_cancel(partition_state->conn);
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
        }
    }
    else if (strcmp(sort, "Priority") == 0) {
        if (mpd_search_add_sort_name(partition_state->conn, "prio", sortdesc) == false) {
            mpd_search_cancel(partition_state->conn);
            return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
                JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
        }
    }
    else {
        MYMPD_LOG_WARN(partition_state->name, "Unknown sort tag: %s", sort);
    }

    unsigned real_limit = limit == 0 ? offset + MPD_PLAYLIST_LENGTH_MAX : offset + limit;
    if (mpd_search_add_window(partition_state->conn, offset, real_limit) == false) {
        mpd_search_cancel(partition_state->conn);
        return jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_DATABASE,
            JSONRPC_SEVERITY_ERROR, "Error creating MPD queue search command");
    }

    if (mpd_search_commit(partition_state->conn)) {
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        buffer = sdscat(buffer, "\"data\":[");
        struct mpd_song *song;
        unsigned total_time = 0;
        long entities_returned = 0;
        while ((song = mpd_recv_song(partition_state->conn)) != NULL) {
            if (entities_returned++) {
                buffer= sdscatlen(buffer, ",", 1);
            }
            buffer = print_queue_entry(partition_state, buffer, tagcols, song);
            total_time += mpd_song_get_duration(song);
            mpd_song_free(song);
        }

        buffer = sdscatlen(buffer, "],", 2);
        buffer = tojson_uint(buffer, "totalTime", total_time, true);
        if (sdslen(expression) == 0) {
            buffer = tojson_llong(buffer, "totalEntities", partition_state->queue_length, true);
        }
        else {
            buffer = tojson_long(buffer, "totalEntities", -1, true);
        }
        buffer = tojson_uint(buffer, "offset", offset, true);
        buffer = tojson_long(buffer, "returnedEntities", entities_returned, false);
        buffer = jsonrpc_end(buffer);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_search_queue_songs") == false) {
        return buffer;
    }

    return buffer;
}

//private functions

/**
 * Prints a queue entry as an json object string
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param tagcols columns to print
 * @param song pointer to mpd song struct
 * @return pointer to buffer
 */
sds print_queue_entry(struct t_partition_state *partition_state, sds buffer, const struct t_tags *tagcols, struct mpd_song *song) {
    buffer = sdscatlen(buffer, "{", 1);
    buffer = tojson_uint(buffer, "id", mpd_song_get_id(song), true);
    buffer = tojson_uint(buffer, "Pos", mpd_song_get_pos(song), true);
    buffer = tojson_uint(buffer, "Priority", mpd_song_get_prio(song), true);
    const struct mpd_audio_format *audioformat = mpd_song_get_audio_format(song);
    buffer = printAudioFormat(buffer, audioformat);
    buffer = sdscatlen(buffer, ",", 1);
    buffer = get_song_tags(buffer, partition_state->mpd_state->feat_tags, tagcols, song);
    const char *uri = mpd_song_get_uri(song);
    buffer = sdscatlen(buffer, ",", 1);
    if (is_streamuri(uri) == true) {
        sds webradio = get_webradio_from_uri(partition_state->mympd_state->config->workdir, uri);
        if (sdslen(webradio) > 0) {
            buffer = sdscat(buffer, "\"webradio\":{");
            buffer = sdscatsds(buffer, webradio);
            buffer = sdscatlen(buffer, "},", 2);
            buffer = tojson_char(buffer, "type", "webradio", false);
        }
        else {
            buffer = tojson_char(buffer, "type", "stream", false);
        }
        FREE_SDS(webradio);
    }
    else {
        buffer = tojson_char(buffer, "type", "song", false);
    }
    if (partition_state->mpd_state->feat_stickers == true) {
        buffer = sdscatlen(buffer, ",", 1);
        buffer = mympd_api_sticker_get_print(buffer, &partition_state->mpd_state->sticker_cache, uri);
    }
    buffer = sdscatlen(buffer, "}", 1);
    return buffer;
}
