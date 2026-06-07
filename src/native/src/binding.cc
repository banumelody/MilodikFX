#include <napi.h>
#include <iostream>

/**
 * Hello World binding - tests that native module compiles correctly
 * This will be extended in Day 5 with actual JUCE audio engine binding
 */

Napi::String HelloWorld(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::String::New(env, "Hello from C++! Native module is working.");
}

/**
 * Initialize - placeholder for audio engine initialization
 * TODO: Replace with actual JUCE audio engine init
 */
Napi::Boolean Initialize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  std::cout << "[Native] Initialize called (placeholder)" << std::endl;
  return Napi::Boolean::New(env, true);
}

/**
 * SetParameter - placeholder for parameter setting
 * TODO: Replace with actual JUCE DSP parameter control
 */
void SetParameter(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() != 3) {
    Napi::TypeError::New(env, "Expected 3 arguments: effect, parameter, value")
      .ThrowAsJavaScriptException();
    return;
  }
  
  if (!info[0].IsString() || !info[1].IsString() || !info[2].IsNumber()) {
    Napi::TypeError::New(env, "Invalid argument types")
      .ThrowAsJavaScriptException();
    return;
  }
  
  std::string effect = info[0].As<Napi::String>();
  std::string parameter = info[1].As<Napi::String>();
  float value = info[2].As<Napi::Number>();
  
  std::cout << "[Native] SetParameter: " << effect << "." << parameter 
            << " = " << value << std::endl;
}

/**
 * GetMeterData - placeholder for meter data retrieval
 * TODO: Replace with actual JUCE meter reading
 */
Napi::Object GetMeterData(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Object obj = Napi::Object::New(env);
  
  obj.Set("inputLevel", Napi::Number::New(env, -12.5f));
  obj.Set("outputLevel", Napi::Number::New(env, -15.3f));
  obj.Set("peakLeft", Napi::Number::New(env, -6.0f));
  obj.Set("peakRight", Napi::Number::New(env, -5.8f));
  
  return obj;
}

/**
 * GetDeviceList - placeholder for device enumeration
 * TODO: Replace with actual JUCE device enumeration
 */
Napi::Array GetDeviceList(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Array devices = Napi::Array::New(env, 2);
  
  // Placeholder device 1
  Napi::Object device1 = Napi::Object::New(env);
  device1.Set("id", Napi::String::New(env, "default-in"));
  device1.Set("name", Napi::String::New(env, "Default Input"));
  device1.Set("isInput", Napi::Boolean::New(env, true));
  devices.Set(uint32_t(0), device1);
  
  // Placeholder device 2
  Napi::Object device2 = Napi::Object::New(env);
  device2.Set("id", Napi::String::New(env, "default-out"));
  device2.Set("name", Napi::String::New(env, "Default Output"));
  device2.Set("isInput", Napi::Boolean::New(env, false));
  devices.Set(uint32_t(1), device2);
  
  return devices;
}

/**
 * Module initialization
 * Exports all available functions to JavaScript
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(
    Napi::String::New(env, "helloWorld"),
    Napi::Function::New(env, HelloWorld)
  );
  
  exports.Set(
    Napi::String::New(env, "initialize"),
    Napi::Function::New(env, Initialize)
  );
  
  exports.Set(
    Napi::String::New(env, "setParameter"),
    Napi::Function::New(env, SetParameter)
  );
  
  exports.Set(
    Napi::String::New(env, "getMeterData"),
    Napi::Function::New(env, GetMeterData)
  );
  
  exports.Set(
    Napi::String::New(env, "getDeviceList"),
    Napi::Function::New(env, GetDeviceList)
  );
  
  return exports;
}

NODE_API_MODULE(audio_binding, Init)
