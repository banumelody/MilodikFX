# Sprint 8 - Day-by-Day Execution Plan

**Status:** ✅ READY TO START  
**Approved Strategy:** SSE + Debouncing + Error Toast  
**Target:** v0.9.0 (fully functional audio editor)  

---

## 🎯 Sprint 8 Overview

**Total Tasks:** 19 subtasks across 5 phases  
**Estimated Duration:** 7 days  
**Go-live Date:** June 14, 2026  

---

## 📅 Day 1: Protocol Design & Decisions Finalized

### Tasks

- [x] Review Sprint 8 planning brief
- [x] Confirm JSON Message Protocol design
- [x] Confirm SSE for meter streaming
- [x] Confirm 50ms parameter debouncing
- [x] Confirm error toast handling

### Deliverables

- [ ] docs/MESSAGE_PROTOCOL.md (message type specifications)
- [ ] C++ Message.h (interface definition)

### Decisions Locked In

```
✅ Message Streaming:    SSE (Server-Sent Events)
✅ Parameter Updates:    Debounce 50ms
✅ Error Handling:       Toast notifications
✅ Testing Strategy:     Load test 100+ req/sec
✅ Performance Target:   < 50ms latency
```

---

## 📅 Day 2-3: C++ Backend Implementation

### Phase 2A: Message Infrastructure (Day 2)

**Create:** `src/api/Message.h`
```cpp
namespace MilodikFX::API {

enum class MessageType {
  GetDeviceList,
  SetDevice,
  SetParameter,
  ParameterChanged,
  MetersUpdate,
  // ... more types
};

struct Message {
  MessageType type;
  std::string id;
  uint64_t timestamp;
  nlohmann::json payload;
  std::optional<Error> error;
};

class MessageCodec {
  static Message fromJson(const std::string& json);
  static std::string toJson(const Message& msg);
};

}
```

**Tasks:**
- [ ] Implement Message struct
- [ ] Implement MessageCodec (serialization)
- [ ] Add to CMakeLists.txt
- [ ] Unit test Message serialization

### Phase 2B: HTTP Endpoints (Day 2-3)

**Modify:** `src/ui/WebServer.cpp`

```cpp
// POST /api/parameters
void handleSetParameter(const Request& req) {
  auto json = nlohmann::json::parse(req.body);
  auto effect = json["effect"].get<std::string>();
  auto param = json["parameter"].get<std::string>();
  auto value = json["value"].get<float>();
  
  // Use lock-free queue
  parameterQueue.enqueue({effect, param, value});
  
  return Response{200, "{\"status\": \"ok\"}"};
}

// GET /api/devices
void handleGetDevices(const Request& req) {
  auto devices = audioDeviceManager.getAvailableDevices();
  auto json = nlohmann::json::array();
  
  for (auto& dev : devices) {
    json.push_back({
      {"id", dev.id},
      {"name", dev.name},
      {"inputs", dev.numInputs},
      {"outputs", dev.numOutputs}
    });
  }
  
  return Response{200, json.dump()};
}

// GET /stream/meters (Server-Sent Events)
void handleMeterStream(const Request& req) {
  auto response = Response{200};
  response.headers["Content-Type"] = "text/event-stream";
  response.headers["Cache-Control"] = "no-cache";
  response.headers["Connection"] = "keep-alive";
  
  // Send meter updates every 50ms
  auto meter = audioEngine.getMeterData();
  response.body += fmt::format("data: {}\n\n", meter.toJson());
  
  return response;
}
```

**Tasks:**
- [ ] Add /api/parameters endpoint
- [ ] Add /api/devices endpoint
- [ ] Add /stream/meters endpoint (SSE)
- [ ] Add /api/audio/start endpoint
- [ ] Add /api/audio/stop endpoint
- [ ] Implement lock-free parameter queue
- [ ] Add request validation
- [ ] Error handling (400, 500 responses)

---

## 📅 Day 4: Frontend Message Bridge Service

### Phase 3: Frontend Integration

**Create:** `frontend/src/services/messageBridge.ts`

```typescript
import { Message, MessageType } from '../types/api';

class MessageBridge {
  private baseUrl = 'http://localhost:3000';
  private requestId = 0;
  
  async setParameter(
    effect: string,
    parameter: string,
    value: number
  ): Promise<void> {
    const response = await fetch(`${this.baseUrl}/api/parameters`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        type: 'setParameter',
        id: `req-${++this.requestId}`,
        timestamp: Date.now(),
        payload: { effect, parameter, value }
      })
    });
    
    if (!response.ok) {
      throw new Error(`Failed to set parameter: ${response.statusText}`);
    }
  }
  
  async getDevices(): Promise<Device[]> {
    const response = await fetch(`${this.baseUrl}/api/devices`);
    if (!response.ok) throw new Error('Failed to get devices');
    return response.json();
  }
  
  async setDevice(deviceId: string): Promise<void> {
    const response = await fetch(`${this.baseUrl}/api/devices/${deviceId}`, {
      method: 'POST'
    });
    if (!response.ok) throw new Error('Failed to set device');
  }
  
  subscribeMeterUpdates(callback: (data: MeterData) => void): () => void {
    const eventSource = new EventSource(`${this.baseUrl}/stream/meters`);
    
    eventSource.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        callback(data);
      } catch (error) {
        console.error('Failed to parse meter data:', error);
      }
    };
    
    eventSource.onerror = () => {
      console.error('Meter stream error, reconnecting...');
      eventSource.close();
      // Implement exponential backoff reconnect
      setTimeout(() => {
        callback(null); // Signal reconnection attempt
      }, 1000);
    };
    
    return () => eventSource.close();
  }
}

export default new MessageBridge();
```

**Tasks:**
- [ ] Create messageBridge.ts
- [ ] Implement setParameter with debouncing
- [ ] Implement getDevices
- [ ] Implement setDevice
- [ ] Implement subscribeMeterUpdates (SSE)
- [ ] Add error handling & retries
- [ ] Add request timeout (5 seconds)
- [ ] Add connection state management
- [ ] Unit tests for messageBridge

---

## 📅 Day 5: Component Integration

### Phase 4: Connect UI to Backend

**Modify:** `frontend/src/components/Knob.tsx`

```typescript
interface KnobProps {
  value: number;
  onChange: (value: number) => void;
  label?: string;
  effect?: string;           // NEW: effect name
  parameter?: string;        // NEW: parameter name
  onError?: (error: string) => void;
}

export function Knob({ value, onChange, effect, parameter, onError }: KnobProps) {
  const [isDragging, setIsDragging] = useState(false);
  const debounceTimer = useRef<NodeJS.Timeout>();
  
  const handleChange = (newValue: number) => {
    onChange(newValue);
    
    // Only send to backend if effect/parameter specified
    if (effect && parameter && isDragging) {
      // Debounce: wait 50ms before sending
      clearTimeout(debounceTimer.current);
      debounceTimer.current = setTimeout(async () => {
        try {
          await messageBridge.setParameter(effect, parameter, newValue);
        } catch (error) {
          onError?.((error as Error).message);
        }
      }, 50);
    }
  };
  
  return (
    <div>
      {/* Knob rendering */}
      {/* ... */}
    </div>
  );
}
```

**Modify:** `frontend/src/components/Meter.tsx`

```typescript
interface MeterProps {
  effect?: string;
  parameter?: string;
  showLevel?: boolean;
  showPeak?: boolean;
}

export function Meter({ effect, parameter }: MeterProps) {
  const [level, setLevel] = useState(0);
  const [peak, setPeak] = useState(0);
  
  useEffect(() => {
    const unsubscribe = messageBridge.subscribeMeterUpdates((data) => {
      if (data) {
        setLevel(data.level || 0);
        setPeak(data.peak || 0);
      }
    });
    
    return unsubscribe;
  }, []);
  
  return (
    <div>
      {/* Meter visualization */}
    </div>
  );
}
```

**Tasks:**
- [ ] Connect Knob component → messageBridge.setParameter
- [ ] Add debouncing (50ms) to Knob
- [ ] Connect Meter component → subscribeMeterUpdates
- [ ] Connect Device selector → setDevice
- [ ] Add loading states (spinner)
- [ ] Add error toast notifications
- [ ] Update UI when API calls fail
- [ ] Add retry logic for failed requests
- [ ] Component integration tests

---

## 📅 Day 6: Testing & Debugging

### Phase 5A: E2E & Performance Testing

**E2E Test Scenario:**
1. User adjusts Overdrive Drive knob from 0.5 → 0.75
2. Verify: Parameter sent to backend
3. Verify: DSP engine receives new value
4. Verify: Audio output changes
5. Verify: Meter updates in UI (< 50ms latency)

**Load Test:**
- Rapid knob dragging (100+ parameter updates/sec)
- Monitor: CPU usage, memory, latency
- Target: < 50ms end-to-end latency

**Resilience Test:**
- Stop WebServer, verify UI handles gracefully
- Restart WebServer, verify auto-reconnect
- Network delay (simulate 500ms latency)

**Tasks:**
- [ ] Create E2E test script
- [ ] Run load test (100+ req/sec)
- [ ] Profile performance (latency, throughput)
- [ ] Test connection failures & recovery
- [ ] Verify no memory leaks
- [ ] Stress test with concurrent connections

### Phase 5B: Bug Fixes & Optimization

**Tasks:**
- [ ] Fix any failing tests
- [ ] Optimize latency (target < 50ms)
- [ ] Optimize memory usage
- [ ] Fix error handling edge cases
- [ ] Improve SSE stream stability
- [ ] Add connection pooling (if needed)
- [ ] Profile and optimize hot paths

---

## 📅 Day 7: Documentation & Release

### Phase 5C: Final Verification

**Tasks:**
- [ ] Run full test suite (unit + E2E)
- [ ] Code review: Backend & Frontend
- [ ] TypeScript: 0 errors
- [ ] ESLint: 0 warnings
- [ ] Performance benchmarks complete
- [ ] All edge cases handled

**Documentation:**
- [ ] Create docs/MESSAGE_PROTOCOL.md
- [ ] Create docs/BACKEND_API.md
- [ ] Create docs/INTEGRATION_GUIDE.md
- [ ] Update docs/prd.md (Sprint 8 complete)
- [ ] API examples in README

**Release:**
- [ ] Tag v0.9.0
- [ ] Create GitHub Release
- [ ] Upload executable
- [ ] Write release notes

---

## 🔄 Task Dependency Chart

```
Day 1: Protocol Design ✓
  ↓
Day 2-3: Backend Endpoints
  ↓
Day 4: Frontend Bridge Service
  ├─ Depends on: Backend Endpoints
  ↓
Day 5: Component Integration
  ├─ Depends on: Frontend Bridge Service
  ↓
Day 6: Testing & Debugging
  ├─ Depends on: All Integration Complete
  ↓
Day 7: Documentation & Release
  └─ Depends on: All Tests Passing
```

---

## 📊 Success Criteria

### Must Have (By Day 6)
- ✅ Parameter sync works (Knob → DSP → Audio)
- ✅ Meter visualization real-time (< 50ms latency)
- ✅ Device selection working
- ✅ Error handling (network failures)
- ✅ All tests passing

### Should Have (By Day 6)
- ✅ Load test passing (100+ req/sec)
- ✅ Connection resilience (auto-reconnect)
- ✅ Performance metrics (< 50ms latency)
- ✅ Full unit test coverage

### Nice to Have
- ✅ Optimistic UI updates
- ✅ Parameter history
- ✅ Undo/redo support

---

## 🚀 Expected Outcome (v0.9.0)

By end of Sprint 8:

**What Users Can Do:**
- ✅ Adjust knobs → hear audio change in real-time
- ✅ Watch meters update as audio plays
- ✅ Select different audio devices
- ✅ Save/load presets from backend
- ✅ Full audio editor functionality

**Technical Achievements:**
- ✅ JSON Message Bridge fully functional
- ✅ Real-time parameter sync (< 50ms)
- ✅ SSE meter streaming
- ✅ Robust error handling
- ✅ Production-ready API

**Application Status:**
FROM: UI-only mockup  
TO: **Fully functional audio editor** 🎵

---

## 📝 Ready to Start?

All planning complete. Ready to begin Day 1 (Protocol Design & Finalization)?

Next steps:
1. Start with `docs/MESSAGE_PROTOCOL.md`
2. Define all message types
3. Create C++ Message infrastructure
4. Proceed through phases

**LET'S GO! 🚀**

