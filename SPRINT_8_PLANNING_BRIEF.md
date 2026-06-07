# Sprint 8 Planning Brief - Backend Bridge Integration

**Status:** Ready for Discussion  
**Target:** v0.9.0  
**Duration:** 2 weeks (estimate)  

---

## 🎯 Sprint 8 Objective

Connect frontend React UI to C++ audio backend via JSON Message Bridge.

**Goal:** Parameter changes in UI → DSP engine updates → Audio processes → Meters update in UI

```
Frontend (React)
    ↓ JSON API
WebServer (C++ HTTP)
    ↓ Lock-free Queue
Audio Engine (C++ DSP)
    ↓ Real-time Processing
Audio Output (ASIO/WASAPI)
```

---

## 📋 Scope: What's Included

### 1. JSON Message Protocol (Required First)

**Define standard message format:**

```typescript
// Message Types
type MessageType = 
  | 'getDeviceList' | 'setDevice' | 'startAudio' | 'stopAudio'
  | 'setParameter' | 'parameterChanged'
  | 'getPresetList' | 'loadPreset' | 'savePreset'
  | 'metersUpdate' | 'effectsChain'

// Message Envelope
interface Message {
  type: MessageType
  id?: string           // request correlation
  timestamp: number
  payload: any
  error?: { code: number; message: string }
}

// Example: Frontend → Backend
{
  "type": "setParameter",
  "id": "req-001",
  "timestamp": 1717773232,
  "payload": {
    "effect": "overdrive",
    "parameter": "drive",
    "value": 0.75
  }
}

// Example: Backend → Frontend
{
  "type": "parameterChanged",
  "id": "req-001",
  "timestamp": 1717773232,
  "payload": {
    "effect": "overdrive",
    "parameter": "drive",
    "value": 0.75
  }
}
```

**Decision Needed:** Should we use REST, WebSocket, or Server-Sent Events (SSE)?

---

### 2. HTTP REST Endpoints (Backend)

**Add to C++ WebServer:**

```
// Device Management
GET  /api/devices                 → List audio devices
POST /api/devices/{id}            → Select device
POST /api/audio/start             → Start audio
POST /api/audio/stop              → Stop audio

// Parameter Control
POST /api/parameters              → Set parameter
GET  /api/parameters              → Get all parameters
GET  /api/parameters/{effect}     → Get effect parameters

// Presets
GET  /api/presets                 → List presets
GET  /api/presets/{id}            → Get preset
POST /api/presets                 → Save preset
DELETE /api/presets/{id}          → Delete preset

// Metering (choose option)
GET  /api/meters                  → Current levels (REST polling)
OR
GET  /stream/meters               → Server-Sent Events (streaming)
OR
WS   ws://localhost:3000/meters   → WebSocket (bidirectional)

// Effects
GET  /api/effects                 → List available effects
POST /api/effects                 → Add effect to chain
DELETE /api/effects/{id}          → Remove effect
PUT  /api/effects/reorder         → Reorder effects
```

**Decision Needed:** REST polling vs SSE vs WebSocket for meter streaming?

---

### 3. Frontend Message Bridge Service

**Create in TypeScript:**

```typescript
// src/services/messageBridge.ts

class MessageBridge {
  private requestId = 0
  
  async setParameter(effect: string, param: string, value: number) {
    return fetch('/api/parameters', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        effect,
        parameter: param,
        value
      })
    })
  }
  
  async getDevices() {
    return fetch('/api/devices').then(r => r.json())
  }
  
  async setDevice(deviceId: string) {
    return fetch(`/api/devices/${deviceId}`, {
      method: 'POST'
    })
  }
  
  // Meter streaming with SSE or polling
  subscribeMeterUpdates(callback: (data: MeterData) => void) {
    // Option A: SSE
    const eventSource = new EventSource('/stream/meters')
    eventSource.onmessage = (e) => callback(JSON.parse(e.data))
    return () => eventSource.close()
    
    // Option B: Polling
    // const interval = setInterval(async () => {
    //   const data = await fetch('/api/meters').then(r => r.json())
    //   callback(data)
    // }, 50)
    // return () => clearInterval(interval)
  }
}

export default new MessageBridge()
```

---

### 4. C++ Backend Implementation

**Modifications to existing code:**

```cpp
// src/MainComponent.cpp

// Add HTTP endpoints to WebServer
webServer.addEndpoint("POST", "/api/parameters", [this](const Request& req) {
  auto json = parseJson(req.body);
  
  std::string effect = json["effect"];
  std::string param = json["parameter"];
  float value = json["value"];
  
  // Set parameter (lock-free atomic)
  if (effect == "overdrive" && param == "drive") {
    overdrive.driveParam.store(value);
  }
  
  return Response{ 200, "{\"status\": \"ok\"}" };
});

// Add meter streaming
webServer.addEndpoint("GET", "/stream/meters", [this](const Request& req) {
  // Server-Sent Events (SSE) implementation
  auto response = Response{ 200 };
  response.headers["Content-Type"] = "text/event-stream";
  response.headers["Cache-Control"] = "no-cache";
  
  // Send meter updates every 50ms
  while (clientConnected) {
    auto levels = audioEngine.getMeterLevels();
    response.body += fmt::format("data: {}\n\n", toJson(levels));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  
  return response;
});
```

---

## 📊 Sprint 8 Tasks Breakdown

### Phase 1: Protocol Design (2 days)

- [ ] Finalize JSON Message Protocol specification
- [ ] Document all message types and payloads
- [ ] Get approval on REST vs SSE vs WebSocket
- [ ] Create Message.h/Message.cpp (serialization)

### Phase 2: Backend Implementation (5 days)

- [ ] Add HTTP endpoints to WebServer
- [ ] Implement parameter setter (lock-free queue)
- [ ] Add meter streaming endpoint (SSE or WebSocket)
- [ ] Device enumeration API
- [ ] Preset CRUD operations
- [ ] Error handling and validation

### Phase 3: Frontend Integration (5 days)

- [ ] Create messageBridge service
- [ ] Connect Knob/Slider components → API
- [ ] Add loading states and error handling
- [ ] Real-time parameter feedback
- [ ] Meter data subscription
- [ ] Device selector integration

### Phase 4: Testing & Verification (3 days)

- [ ] Unit tests for messageBridge
- [ ] E2E test: Parameter flow (UI → DSP → Feedback)
- [ ] Load testing (100+ parameter changes/sec)
- [ ] Connection resilience testing
- [ ] Performance benchmarking

### Phase 5: Documentation & Polish (2 days)

- [ ] API specification document
- [ ] Integration examples
- [ ] Troubleshooting guide
- [ ] Performance metrics report

---

## 🔄 Data Flow Example

### User adjusts Overdrive Drive knob from 0.5 to 0.75:

```
1. Frontend: User drags Knob
   ↓
2. React: onValueChange fired with 0.75
   ↓
3. messageBridge.setParameter("overdrive", "drive", 0.75)
   ↓
4. POST /api/parameters with { effect: "overdrive", parameter: "drive", value: 0.75 }
   ↓
5. C++ WebServer: Receives POST request
   ↓
6. C++ Backend: overdrive.driveParam.store(0.75)  // lock-free atomic
   ↓
7. Audio Callback: Reads current value, applies DSP
   ↓
8. Output: Sound changes with new drive value
   ↓
9. C++ Meter: Calculates output level → 0.85 (example)
   ↓
10. SSE Stream: Sends "data: {level: 0.85, peak: 0.92}\n\n"
    ↓
11. Frontend: Receives meter update
    ↓
12. React: Meter component re-renders with new values
    ↓
13. User sees visual feedback immediately
```

---

## 🎯 Key Decisions Needed

### Decision 1: Message Streaming Strategy

**Option A: REST Polling**
```javascript
// Frontend polls every 50ms
setInterval(async () => {
  const meters = await fetch('/api/meters').then(r => r.json());
  updateMeters(meters);
}, 50);
```
- ✅ Simple to implement
- ✅ Works everywhere (no WebSocket support needed)
- ❌ Higher bandwidth (constant requests)
- ❌ Latency: 25-50ms average delay

**Option B: Server-Sent Events (SSE) [RECOMMENDED]**
```javascript
// Backend streams updates
const eventSource = new EventSource('/stream/meters');
eventSource.onmessage = (e) => updateMeters(JSON.parse(e.data));
```
- ✅ Lower bandwidth (only sends when data changes)
- ✅ Real-time (< 5ms latency)
- ✅ Simpler than WebSocket
- ✅ One-way is perfect for meter data
- ❌ HTTP-only (good for localhost)

**Option C: WebSocket**
```javascript
// Bidirectional real-time
const ws = new WebSocket('ws://localhost:3000/meters');
ws.onmessage = (e) => updateMeters(JSON.parse(e.data));
```
- ✅ Full duplex (can send and receive)
- ✅ Lowest latency (< 2ms)
- ✅ Most scalable
- ❌ More complex to implement
- ❌ Overkill for one-way meter data

**RECOMMENDATION:** Use SSE for meters (real-time, simple). REST for parameters (low frequency).

---

### Decision 2: Parameter Sync Strategy

**Option A: Every Change**
- Frontend sends parameter on every change
- Best for: Knob dragging, slider interactions
- Issue: 100+ requests/sec during drag
- Need: Rate limiting or batching

**Option B: On Release**
- Frontend sends parameter only when user stops dragging
- Best for: Minimizing requests
- Issue: No real-time feedback during adjustment
- Problem: Doesn't match audio feedback

**Option C: Debounced [RECOMMENDED]**
- Frontend batches changes every 50ms
- Best for: Balance between responsiveness and bandwidth
- Effect: 50ms UI delay (imperceptible)
- Bandwidth: ~20 requests/sec during drag (good)

**RECOMMENDATION:** Debounce parameter updates to 50ms batches.

---

### Decision 3: Error Handling Strategy

**Option A: Silent Retry**
- If request fails, retry automatically
- User never sees errors
- Risk: User doesn't know parameter didn't apply

**Option B: Show Error [RECOMMENDED]**
- If request fails, show toast notification
- User knows something went wrong
- Can retry or cancel

**Option C: Fallback to Cache**
- If request fails, use cached value
- Show warning to user
- Risky: UI and backend out of sync

**RECOMMENDATION:** Show errors to user, with manual retry option.

---

## 📈 Success Criteria

### Must Have (Phase 1-3)
- ✅ Parameter sync works (knob → DSP → audio change)
- ✅ Meter data streaming (real-time visualization)
- ✅ Device selection (user can pick audio device)
- ✅ Zero errors in strict TypeScript mode
- ✅ All unit tests passing

### Should Have (Phase 4)
- ✅ Load testing (100+ requests/sec stable)
- ✅ Error handling (network failures, timeouts)
- ✅ Performance benchmarks (latency < 50ms)
- ✅ Connection resilience (auto-reconnect)

### Nice to Have (Phase 5)
- ✅ Optimistic updates (instant UI feedback)
- ✅ Undo/redo for parameter changes
- ✅ Parameter change history
- ✅ WebSocket upgrade path

---

## 🚀 Expected Outcome (v0.9.0)

By end of Sprint 8:

✅ Fully functional parameter control  
✅ Real-time audio feedback  
✅ Live meter visualization  
✅ Device management  
✅ Preset save/load from backend  
✅ Full Message Bridge integration  
✅ Production-ready backend API  

**Application Status:** From UI-only → **Fully Functional Audio Editor**

---

## 📝 Questions for Discussion

1. **Message Streaming:** REST polling vs SSE vs WebSocket?
   - My recommendation: **SSE for meters, REST for parameters**

2. **Parameter Debouncing:** Batch every 50ms?
   - My recommendation: **Yes, for UI smoothness**

3. **Error Handling:** Show errors or silent retry?
   - My recommendation: **Show errors to user**

4. **Priority Order:** Implement in sequence 1→2→3→4→5?
   - My recommendation: **Yes, one phase per day**

5. **Testing Approach:** Load test with concurrent connections?
   - My recommendation: **Yes, test with 10+ simultaneous connections**

6. **Documentation:** API docs or code comments?
   - My recommendation: **Both - spec document + inline comments**

---

## ⏱️ Timeline Estimate

```
Day 1: Protocol design + decision finalization
Day 2: C++ WebServer endpoints implementation
Day 3: Frontend messageBridge service
Day 4: Component integration (Knobs, Meters, Device)
Day 5: Testing & error handling
Day 6: Performance optimization & load testing
Day 7: Documentation & polish
```

**Total:** ~1 week (7 days) for full Sprint 8

---

## 🎯 Next Steps

**To proceed, we need:**

1. ✅ Approval on JSON Message Protocol design
2. ✅ Decision: SSE or WebSocket for meters?
3. ✅ Confirmation on parameter debouncing strategy
4. ✅ Agreement on error handling approach
5. ✅ Go-ahead to start Phase 1

**Should we:**
- A. Start immediately with recommended strategy?
- B. Discuss and customize any decisions first?
- C. Review this plan in detail before proceeding?

---

