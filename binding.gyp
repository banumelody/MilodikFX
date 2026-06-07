{
  "targets": [
    {
      "target_name": "audio_binding",
      "sources": [
        "src/native/src/binding.cc"
      ],
      "include_dirs": [
        "<!(node -p \"require('node-addon-api').include_dir\")",
        "src/native/include"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags": [
        "-Wall",
        "-Wextra"
      ],
      "cflags!": [
        "-fno-exceptions"
      ],
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "GCC_ENABLE_OBJC_EXCEPTIONS": "YES",
        "CLANG_ENABLE_OBJC_ARC": "YES"
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "RuntimeLibrary": 2
        }
      },
      "conditions": [
        [
          "OS == 'win'",
          {
            "libraries": [
              "ws2_32.lib",
              "winmm.lib",
              "ole32.lib",
              "user32.lib"
            ],
            "msbuild_settings": {
              "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "RuntimeLibrary": 2,
                "WarningLevel": 3
              }
            }
          }
        ],
        [
          "OS == 'mac'",
          {
            "libraries": [
              "-framework CoreAudio",
              "-framework CoreFoundation",
              "-framework AudioToolbox"
            ]
          }
        ],
        [
          "OS == 'linux'",
          {
            "libraries": [
              "-lasound"
            ]
          }
        ]
      ]
    }
  ]
}
