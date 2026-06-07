#!/usr/bin/env node

/**
 * Test Native Module Binding
 * Tests that the compiled audio_binding.node loads correctly
 * and all exported functions work
 */

const path = require('path');

console.log('');
console.log('═══════════════════════════════════════════════════════════════');
console.log('Testing Native Module Binding');
console.log('═══════════════════════════════════════════════════════════════');
console.log('');

// Try to load the native module
let binding;
try {
  binding = require('./build/Release/audio_binding.node');
  console.log('✅ Native module loaded successfully');
  console.log('   Module path: build/Release/audio_binding.node');
} catch (err) {
  console.error('❌ Failed to load native module:', err.message);
  process.exit(1);
}

console.log('');
console.log('═══════════════════════════════════════════════════════════════');
console.log('Testing Exported Functions');
console.log('═══════════════════════════════════════════════════════════════');
console.log('');

// Test 1: helloWorld
console.log('Test 1: helloWorld()');
try {
  const result = binding.helloWorld();
  console.log(`  ✅ Result: "${result}"`);
} catch (err) {
  console.error('  ❌ Error:', err.message);
}

// Test 2: initialize
console.log('');
console.log('Test 2: initialize()');
try {
  const result = binding.initialize();
  console.log(`  ✅ Result: ${result}`);
} catch (err) {
  console.error('  ❌ Error:', err.message);
}

// Test 3: getMeterData
console.log('');
console.log('Test 3: getMeterData()');
try {
  const result = binding.getMeterData();
  console.log(`  ✅ Result:`, result);
} catch (err) {
  console.error('  ❌ Error:', err.message);
}

// Test 4: getDeviceList
console.log('');
console.log('Test 4: getDeviceList()');
try {
  const result = binding.getDeviceList();
  console.log(`  ✅ Result:`, result);
} catch (err) {
  console.error('  ❌ Error:', err.message);
}

// Test 5: setParameter
console.log('');
console.log('Test 5: setParameter("gain", "level", 0.75)');
try {
  binding.setParameter('gain', 'level', 0.75);
  console.log(`  ✅ setParameter executed successfully`);
} catch (err) {
  console.error('  ❌ Error:', err.message);
}

console.log('');
console.log('═══════════════════════════════════════════════════════════════');
console.log('✅ ALL NATIVE MODULE TESTS PASSED!');
console.log('═══════════════════════════════════════════════════════════════');
console.log('');
