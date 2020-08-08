# FAQ

## Pre-requisites

Compile `mkspiffs` in this submodule on git. Use:

```
git submodule update --init
make dist CPPFLAGS="-DSPIFFS_OBJ_META_LEN=4 -DSPIFFS_ALIGNED_OBJECT_INDEX_TABLES=1" BUILD_CONFIG_NAME=-custom
```
