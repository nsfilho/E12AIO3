# FAQ

## Pre-requisites

Compile `mkspiffs` in this submodule on git. Use:

```
make dist CPPFLAGS="-DSPIFFS_OBJ_META_LEN=4 -DSPIFFS_ALIGNED_OBJECT_INDEX_TABLES=1" BUILD_CONFIG_NAME=-custom
```

## Compiling in MacOS

Problably you will need to change Makefile of `mkspiffs` to remove i386 if you not have cross-compiler. From:

```Makefile

ifeq ($(TARGET_OS),osx)
	TARGET_CFLAGS   = -mmacosx-version-min=10.7 -arch i386 -arch x86_64
	TARGET_CXXFLAGS = -mmacosx-version-min=10.7 -arch i386 -arch x86_64 -stdlib=libc++
	TARGET_LDFLAGS  = -mmacosx-version-min=10.7 -arch i386 -arch x86_64 -stdlib=libc++
endif

```

**TO**

```Makefile
ifeq ($(TARGET_OS),osx)
	TARGET_CFLAGS   = -mmacosx-version-min=10.7 -arch x86_64
	TARGET_CXXFLAGS = -mmacosx-version-min=10.7 -arch x86_64 -stdlib=libc++
	TARGET_LDFLAGS  = -mmacosx-version-min=10.7 -arch x86_64 -stdlib=libc++
endif
```
