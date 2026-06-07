#!/usr/bin/env node

/**
 * MilodikFX Electron Troubleshooting Script
 * Debug halaman blank di Electron app
 */

const path = require('path');
const fs = require('fs');

console.log('\n╔════════════════════════════════════════════════════════════╗');
console.log('║        🔍 MilodikFX Electron Troubleshooting Debug        ║');
console.log('╚════════════════════════════════════════════════════════════╝\n');

// 1. Check production build
console.log('1️⃣  Checking React Production Build...\n');

const distPath = path.join(__dirname, 'frontend/dist/index.html');
if (fs.existsSync(distPath)) {
  console.log('   ✅ Production build found');
  console.log(`      Path: ${distPath}`);
  
  const stats = fs.statSync(distPath);
  console.log(`      Size: ${stats.size} bytes`);
  
  // Check for required files
  const requiredFiles = [
    'frontend/dist/index.html',
    'frontend/dist/assets'
  ];
  
  console.log('\n   Checking assets...');
  const assetsPath = path.join(__dirname, 'frontend/dist/assets');
  if (fs.existsSync(assetsPath)) {
    const assets = fs.readdirSync(assetsPath);
    console.log(`      ✅ Assets folder found (${assets.length} files)`);
  } else {
    console.log('      ❌ Assets folder NOT found!');
  }
} else {
  console.log('   ❌ Production build NOT found!');
  console.log(`      Expected: ${distPath}`);
  console.log('\n   🔧 FIX: Run `npm run build` in frontend/ folder');
}

// 2. Check electron/main.js
console.log('\n2️⃣  Checking Electron Main Process...\n');

const mainPath = path.join(__dirname, 'electron/main.js');
if (fs.existsSync(mainPath)) {
  console.log('   ✅ electron/main.js found');
  
  const content = fs.readFileSync(mainPath, 'utf8');
  
  if (content.includes('frontend/dist/index.html')) {
    console.log('   ✅ Production path configured');
  } else {
    console.log('   ⚠️  Production path check: verify in electron/main.js');
  }
  
  if (content.includes('localhost:3000')) {
    console.log('   ✅ Dev server fallback configured');
  }
} else {
  console.log('   ❌ electron/main.js NOT found!');
}

// 3. Check preload.js
console.log('\n3️⃣  Checking Preload Bridge...\n');

const preloadPath = path.join(__dirname, 'electron/preload.js');
if (fs.existsSync(preloadPath)) {
  console.log('   ✅ electron/preload.js found');
} else {
  console.log('   ❌ electron/preload.js NOT found!');
}

// 4. Check package.json scripts
console.log('\n4️⃣  Checking npm Scripts...\n');

const pkgPath = path.join(__dirname, 'package.json');
const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));

console.log('   Available scripts:');
Object.keys(pkg.scripts || {}).forEach(key => {
  console.log(`      • ${key}`);
});

// 5. Development mode instructions
console.log('\n5️⃣  DEVELOPMENT MODE (Recommended for testing)...\n');

console.log('   To run in DEV mode with hot reload:');
console.log('');
console.log('   Terminal 1 - Start React dev server:');
console.log('      cd frontend');
console.log('      npm run dev');
console.log('');
console.log('   Terminal 2 - Start Electron:');
console.log('      npm run dev:electron');
console.log('');
console.log('   This will:');
console.log('      ✓ Start Vite dev server on localhost:3000');
console.log('      ✓ Start Electron pointing to localhost:3000');
console.log('      ✓ Enable hot module reload');
console.log('      ✓ Open DevTools for debugging');

// 6. Production mode instructions
console.log('\n6️⃣  PRODUCTION MODE (Current)...\n');

console.log('   To run portable .exe:');
console.log('      ./dist/MilodikFX 0.9.0-electron.exe');
console.log('');
console.log('   This should load from frontend/dist/');
console.log('   If blank, check:');
console.log('      1. Right-click → Inspect');
console.log('      2. Check Console tab for errors');
console.log('      3. Check Network tab - see if files load');

// 7. Common issues and fixes
console.log('\n7️⃣  COMMON ISSUES & FIXES...\n');

const issues = [
  {
    issue: 'Halaman Blank Putih',
    causes: [
      'React dev server (localhost:3000) tidak running di dev mode',
      'Production build tidak di-generate (frontend/dist/ kosong)',
      'Path ke index.html salah di electron/main.js',
      'Assets folder tidak ada atau kosong'
    ],
    fixes: [
      '✓ Pastikan npm run build sudah dijalankan',
      '✓ Pastikan frontend/dist/index.html ada',
      '✓ Coba dev mode: npm run dev:electron (dengan React dev server)',
      '✓ Check DevTools: Ctrl+Shift+I'
    ]
  },
  {
    issue: 'DevTools tidak membuka',
    causes: [
      'DevTools disabled di production mode'
    ],
    fixes: [
      '✓ Gunakan dev mode untuk debugging',
      '✓ Edit electron/main.js untuk enable DevTools di prod'
    ]
  },
  {
    issue: 'App crash saat startup',
    causes: [
      'Preload script error',
      'Native module loading error',
      'Missing asset'
    ],
    fixes: [
      '✓ Check package.json dependencies',
      '✓ npm install di root dan frontend/',
      '✓ Rebuild: npm run dist:win'
    ]
  }
];

issues.forEach((item, idx) => {
  console.log(`   Issue ${idx + 1}: ${item.issue}`);
  console.log('   Causes:');
  item.causes.forEach(cause => console.log(`      • ${cause}`));
  console.log('   Fixes:');
  item.fixes.forEach(fix => console.log(`      ${fix}`));
  console.log('');
});

console.log('═══════════════════════════════════════════════════════════\n');
console.log('✅ Troubleshooting report complete\n');
console.log('Next step: Review issues above and apply fixes\n');
