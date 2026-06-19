#pragma once

#include "midi_player.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FuriMessageQueue FuriMessageQueue;
typedef struct Gui Gui;
typedef struct FlipMidiUi FlipMidiUi;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FlipMidiUiEventBack = 0,
    FlipMidiUiEventPlayPause,
    FlipMidiUiEventVolumeUp,
    FlipMidiUiEventVolumeDown,
} FlipMidiUiEventType;

typedef struct {
    FlipMidiUiEventType type;
} FlipMidiUiEvent;

typedef struct {
    MidiPlayerStatus status;
    uint32_t tick;
    uint8_t current_note;
    uint8_t volume_percent;
    bool has_current_note;
    char message[64];
    char filename[40];
} FlipMidiUiState;

FlipMidiUi* midi_ui_alloc(FuriMessageQueue* events);
void midi_ui_attach(FlipMidiUi* ui, Gui* gui);
void midi_ui_update(FlipMidiUi* ui, const FlipMidiUiState* state);
void midi_ui_free(FlipMidiUi* ui, Gui* gui);

#ifdef __cplusplus
}
#endif
