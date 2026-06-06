#pragma once

#include <string>
#include <vector>

namespace milodikfx::ipc
{

// ============================================================================
// Message Type Enumeration
// ============================================================================

enum class MessageType
{
    // Device management
    DEVICE_LIST,
    DEVICE_SELECT,
    DEVICE_CHANGED,
    DEVICE_INFO,

    // Preset management
    PRESET_SAVE,
    PRESET_LOAD,
    PRESET_DELETE,
    PRESET_LIST,
    PRESET_SAVED,
    PRESET_LOADED,

    // Parameter control
    PARAMETER_SET,
    PARAMETER_GET,
    PARAMETER_CHANGED,

    // Effect chain
    EFFECT_ADD,
    EFFECT_REMOVE,
    EFFECT_REORDER,
    EFFECT_ENABLED,
    EFFECT_LIST,

    // Audio metering
    AUDIO_METERING,

    // Tuner
    TUNER_FREQUENCY,

    // Metronome
    METRONOME_TEMPO,
    METRONOME_TAP,

    // Scene management
    SCENE_SAVE,
    SCENE_LOAD,
    SCENE_LIST,
    SCENE_RECALL,

    // Status
    SERVER_READY,
    ERROR,
    UNKNOWN
};

// ============================================================================
// Message Type Conversion
// ============================================================================

inline const char* messageTypeToString(MessageType type)
{
    switch (type)
    {
        case MessageType::DEVICE_LIST: return "device.list";
        case MessageType::DEVICE_SELECT: return "device.select";
        case MessageType::DEVICE_CHANGED: return "device.changed";
        case MessageType::DEVICE_INFO: return "device.info";
        case MessageType::PRESET_SAVE: return "preset.save";
        case MessageType::PRESET_LOAD: return "preset.load";
        case MessageType::PRESET_DELETE: return "preset.delete";
        case MessageType::PRESET_LIST: return "preset.list";
        case MessageType::PRESET_SAVED: return "preset.saved";
        case MessageType::PRESET_LOADED: return "preset.loaded";
        case MessageType::PARAMETER_SET: return "parameter.set";
        case MessageType::PARAMETER_GET: return "parameter.get";
        case MessageType::PARAMETER_CHANGED: return "parameter.changed";
        case MessageType::EFFECT_ADD: return "effect.add";
        case MessageType::EFFECT_REMOVE: return "effect.remove";
        case MessageType::EFFECT_REORDER: return "effect.reorder";
        case MessageType::EFFECT_ENABLED: return "effect.enabled";
        case MessageType::EFFECT_LIST: return "effect.list";
        case MessageType::AUDIO_METERING: return "audio.metering";
        case MessageType::TUNER_FREQUENCY: return "tuner.frequency";
        case MessageType::METRONOME_TEMPO: return "metronome.tempo";
        case MessageType::METRONOME_TAP: return "metronome.tap";
        case MessageType::SCENE_SAVE: return "scene.save";
        case MessageType::SCENE_LOAD: return "scene.load";
        case MessageType::SCENE_LIST: return "scene.list";
        case MessageType::SCENE_RECALL: return "scene.recall";
        case MessageType::SERVER_READY: return "server.ready";
        case MessageType::ERROR: return "error";
        default: return "unknown";
    }
}

inline MessageType stringToMessageType(const std::string& str)
{
    if (str == "device.list") return MessageType::DEVICE_LIST;
    if (str == "device.select") return MessageType::DEVICE_SELECT;
    if (str == "device.changed") return MessageType::DEVICE_CHANGED;
    if (str == "device.info") return MessageType::DEVICE_INFO;
    if (str == "preset.save") return MessageType::PRESET_SAVE;
    if (str == "preset.load") return MessageType::PRESET_LOAD;
    if (str == "preset.delete") return MessageType::PRESET_DELETE;
    if (str == "preset.list") return MessageType::PRESET_LIST;
    if (str == "preset.saved") return MessageType::PRESET_SAVED;
    if (str == "preset.loaded") return MessageType::PRESET_LOADED;
    if (str == "parameter.set") return MessageType::PARAMETER_SET;
    if (str == "parameter.get") return MessageType::PARAMETER_GET;
    if (str == "parameter.changed") return MessageType::PARAMETER_CHANGED;
    if (str == "effect.add") return MessageType::EFFECT_ADD;
    if (str == "effect.remove") return MessageType::EFFECT_REMOVE;
    if (str == "effect.reorder") return MessageType::EFFECT_REORDER;
    if (str == "effect.enabled") return MessageType::EFFECT_ENABLED;
    if (str == "effect.list") return MessageType::EFFECT_LIST;
    if (str == "audio.metering") return MessageType::AUDIO_METERING;
    if (str == "tuner.frequency") return MessageType::TUNER_FREQUENCY;
    if (str == "metronome.tempo") return MessageType::METRONOME_TEMPO;
    if (str == "metronome.tap") return MessageType::METRONOME_TAP;
    if (str == "scene.save") return MessageType::SCENE_SAVE;
    if (str == "scene.load") return MessageType::SCENE_LOAD;
    if (str == "scene.list") return MessageType::SCENE_LIST;
    if (str == "scene.recall") return MessageType::SCENE_RECALL;
    if (str == "server.ready") return MessageType::SERVER_READY;
    if (str == "error") return MessageType::ERROR;
    return MessageType::UNKNOWN;
}

// ============================================================================
// Audio Metrics Structure
// ============================================================================

struct AudioMetrics
{
    float inputLevelDb = 0.0f;
    float outputLevelDb = 0.0f;
    float cpuLoad = 0.0f;
    float latencyMs = 0.0f;
    float peakInputLevelDb = 0.0f;
    float peakOutputLevelDb = 0.0f;
};

// ============================================================================
// Device Info Structure
// ============================================================================

struct DeviceInfo
{
    std::string id;
    std::string name;
    int sampleRate = 44100;
    int bufferSize = 256;
    float latencyMs = 0.0f;
    bool isInput = false;
};

// ============================================================================
// Parameter Update Structure
// ============================================================================

struct ParameterUpdate
{
    std::string effectId;
    std::string paramName;
    float value = 0.0f;
};

// ============================================================================
// Effect Info Structure
// ============================================================================

struct EffectInfo
{
    std::string id;
    std::string type;
    std::string name;
    bool enabled = true;
    std::vector<ParameterUpdate> parameters;
};

// ============================================================================
// Scene Definition Structure
// ============================================================================

struct SceneInfo
{
    int sceneNumber = 1;
    std::string name;
    std::vector<EffectInfo> effects;
    std::string notes;
};

} // namespace milodikfx::ipc
