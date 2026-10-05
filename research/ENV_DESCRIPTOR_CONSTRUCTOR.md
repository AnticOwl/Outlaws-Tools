# Snowdrop environment descriptor constructor

Validated against the supplied `Outlaws.exe`.

## Constructor

- Function start: `Outlaws.exe+0x178A240` (`0x14178A240` for the analyzed image base)
- Runtime function range from PE exception metadata begins at RVA `0x178A240`.
- Descriptors are emitted sequentially at a `0x40` byte stride.

## Descriptor layout

```text
+00  uint32   value/flags (exact semantic still under validation)
+04  uint16   variable ID
+06  uint16   padding
+08  char*    variable name
+10  uint64   initial/default scalar payload
+18  uint32   reserved
+1C  uint8    reserved
+20  void*    type info
+28  uint8    has min
+2C  float    min
+30  uint8    has max
+34  float    max
+38  uint8    extra flag
+40           next descriptor
```

## Confirmed descriptors

### Env_ExposureTarget2

- ID: `0x52`
- Descriptor offset from owner: `+0x1480`
- Name field: owner `+0x1488`
- Name RVA: `0x5C98F88`
- Constructor writes min `0.0` and max `100.0`.

Relevant code:

```text
14178CB59  mov eax,52h
14178CB76  mov dword ptr [rcx+1474h],42C80000h ; 100.0
14178CB87  mov dword ptr [rcx+1480h],0
14178CB91  mov word ptr [rcx+1484h],ax
14178CB98  lea rax,[Env_ExposureTarget2]
14178CB9F  mov [rcx+1488h],rax
```

The instructions before `+1480` belong to the previous descriptor; this is why the earlier provisional `+0x1450` alignment was incorrect.

### Env_GameplayRainAmount

- ID: `0xD0`
- Descriptor offset: `+0x3400`
- Name field: `+0x3408`
- Name RVA: `0x5C99DC0`

### Env_GameplayFogAmount

- ID: `0xD1`
- Descriptor offset: `+0x3440`
- Name field: `+0x3448`
- Name RVA: `0x5C99DD8`

## Runtime owner strategy

Rather than relying on a static owner pointer, the tool now locates the live descriptor owner by scanning readable committed pages for the relocated pointer to `Env_ExposureTarget2`. A candidate owner is accepted only when Exposure, Rain and Fog all match their expected IDs and relocated name pointers at their known offsets.

This intentionally resolves descriptors only. It does **not** assume that editing descriptor fields changes live environment values. The runtime value storage / setter path still needs validation before writes are enabled.
