#include "midi_player.h"
#include "midi_ui.h"

#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <storage/storage.h>

#include <stdio.h>
#include <string.h>

#define FLIPMIDI_DATA_DIR "/ext/apps_data/flipmidi"
#define FLIPMIDI_SONGS_DIR "/ext/apps_data/flipmidi/songs"
#define FLIPMIDI_DEFAULT_PATH "/ext/apps_data/flipmidi/songs/example.mid"
#define FLIPMIDI_VOLUME_STEP_PERCENT (10U)

static const char* flipmidi_basename(const char* path) {
    const char* basename = path;
    for(const char* cursor = path; *cursor != '\0'; cursor++) {
        if(*cursor == '/') {
            basename = cursor + 1;
        }
    }
    return basename;
}

static void flipmidi_make_ui_state(
    const MidiPlayerState* player_state,
    const char* song_path,
    FlipMidiUiState* ui_state) {
    memset(ui_state, 0, sizeof(*ui_state));
    ui_state->status = player_state->status;
    ui_state->tick = player_state->tick;
    ui_state->current_note = player_state->current_note;
    ui_state->volume_percent = player_state->volume_percent;
    ui_state->has_current_note = player_state->has_current_note;
    snprintf(ui_state->message, sizeof(ui_state->message), "%s", player_state->message);
    snprintf(ui_state->filename, sizeof(ui_state->filename), "%s", flipmidi_basename(song_path));
}

static bool flipmidi_select_song(Storage* storage, DialogsApp* dialogs, FuriString* selected_path) {
    if(!storage || !dialogs || !selected_path) {
        return false;
    }

    storage_common_mkdir(storage, FLIPMIDI_DATA_DIR);
    storage_common_mkdir(storage, FLIPMIDI_SONGS_DIR);

    FuriString* browser_path = furi_string_alloc_set_str(FLIPMIDI_DEFAULT_PATH);
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".mid", NULL);
    options.base_path = FLIPMIDI_SONGS_DIR;
    options.hide_dot_files = true;
    options.skip_assets = true;
    options.hide_ext = false;

    const bool selected =
        dialog_file_browser_show(dialogs, selected_path, browser_path, &options) &&
        !furi_string_empty(selected_path);

    furi_string_free(browser_path);
    return selected;
}

static void flipmidi_adjust_volume(MidiPlayer* player, bool increase) {
    if(!player) {
        return;
    }

    const MidiPlayerState state = midi_player_get_state(player);
    uint8_t volume = state.volume_percent;
    if(increase) {
        volume = volume + FLIPMIDI_VOLUME_STEP_PERCENT > 100U ?
                     100U :
                     volume + FLIPMIDI_VOLUME_STEP_PERCENT;
    } else {
        volume = volume < FLIPMIDI_VOLUME_STEP_PERCENT ? 0U :
                                                        volume - FLIPMIDI_VOLUME_STEP_PERCENT;
    }

    MidiPlayerCommand command = {
        .type = MidiPlayerCommandSetVolume,
        .volume = (float)volume / 100.0f,
    };
    midi_player_send_command(player, &command);
}

static void flipmidi_toggle_play_pause(MidiPlayer* player) {
    if(!player) {
        return;
    }

    const MidiPlayerState state = midi_player_get_state(player);
    MidiPlayerCommand command = {
        .type = state.status == MidiPlayerStatusPaused ? MidiPlayerCommandResume :
                                                         MidiPlayerCommandPause,
        .volume = 0.0f,
    };

    if(state.status == MidiPlayerStatusPlaying || state.status == MidiPlayerStatusPaused) {
        midi_player_send_command(player, &command);
    }
}

int32_t flipmidi_app(void* context) {
    (void)context;

    FuriMessageQueue* ui_events = furi_message_queue_alloc(4U, sizeof(FlipMidiUiEvent));
    Gui* gui = furi_record_open(RECORD_GUI);
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* selected_path = furi_string_alloc();
    FlipMidiUi* ui = NULL;
    MidiPlayer* player = NULL;

    if(!flipmidi_select_song(storage, dialogs, selected_path)) {
        goto cleanup;
    }

    ui = midi_ui_alloc(ui_events);
    player = midi_player_alloc(storage);
    if(ui) {
        midi_ui_attach(ui, gui);
    }

    if(player) {
        if(!midi_player_start_path(player, furi_string_get_cstr(selected_path))) {
            MidiPlayerState state = midi_player_get_state(player);
            state.status = MidiPlayerStatusError;
            snprintf(state.message, sizeof(state.message), "%s", "Player start failed");
            FlipMidiUiState ui_state;
            flipmidi_make_ui_state(&state, furi_string_get_cstr(selected_path), &ui_state);
            midi_ui_update(ui, &ui_state);
        }
    }

    bool exit_requested = false;
    while(!exit_requested) {
        FlipMidiUiEvent event;
        if(furi_message_queue_get(ui_events, &event, 250U) == FuriStatusOk) {
            if(event.type == FlipMidiUiEventBack) {
                exit_requested = true;
            } else if(event.type == FlipMidiUiEventPlayPause) {
                flipmidi_toggle_play_pause(player);
            } else if(event.type == FlipMidiUiEventVolumeUp) {
                flipmidi_adjust_volume(player, true);
            } else if(event.type == FlipMidiUiEventVolumeDown) {
                flipmidi_adjust_volume(player, false);
            }
        }

        if(player && ui) {
            const MidiPlayerState state = midi_player_get_state(player);
            FlipMidiUiState ui_state;
            flipmidi_make_ui_state(&state, furi_string_get_cstr(selected_path), &ui_state);
            midi_ui_update(ui, &ui_state);
        }
    }

cleanup:
    if(player) {
        MidiPlayerCommand stop_command = {.type = MidiPlayerCommandStop, .volume = 0.0f};
        midi_player_send_command(player, &stop_command);
        midi_player_free(player);
    }
    if(ui) {
        midi_ui_free(ui, gui);
    }
    if(storage) {
        furi_record_close(RECORD_STORAGE);
    }
    if(dialogs) {
        furi_record_close(RECORD_DIALOGS);
    }
    if(gui) {
        furi_record_close(RECORD_GUI);
    }
    if(selected_path) {
        furi_string_free(selected_path);
    }
    if(ui_events) {
        furi_message_queue_free(ui_events);
    }

    return 0;
}
