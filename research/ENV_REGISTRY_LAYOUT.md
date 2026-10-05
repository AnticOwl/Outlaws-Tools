# Environment registry layout notes

Static analysis target: supplied `Outlaws.exe`.

## Regular descriptor blocks

The environment-variable initialization code shows repeated records at a regular spacing of approximately `0x40` bytes inside a larger object.

Two high-confidence examples:

### Env_ExposureTarget2

Name pointer written from xref:

`0x14178CB98 -> "Env_ExposureTarget2"`

Observed record region begins around object offset `0x1468` and contains:

- enable/validity-like byte flags
- float `0.0`
- float `100.0`
- another flag/value pair
- 16-bit ID `0x52`
- name pointer at object offset `0x1488`
- additional pointer/state fields

Adjacent records:

- `Env_ExtraWindFog`
- `Env_ExposureTarget2`
- `Env_ExposureTargetLerp2`

This strongly indicates a generated registry/descriptor table rather than ad-hoc string usage.

### Env_GameplayRainAmount

Name pointer written from xref:

`0x1417906A1 -> "Env_GameplayRainAmount"`

Observed ID:

`0xD0`

Name pointer stored at object offset `0x3408`.

Adjacent records include:

- `Env_LocalExposureBrightnessCutoff`
- `Env_GameplayRainAmount`
- `Env_GameplayFogAmount`
- `Env_GameplayCloudCover`

`Env_GameplayFogAmount` and `Env_GameplayCloudCover` show an inline `1.0f` in the corresponding metadata region. `Env_GameplayRainAmount` differs slightly in its flags/metadata, so field semantics must be live-validated before labeling every slot min/max/default.

## Secondary lookup path

`Env_GameplayRainAmount` is also referenced at:

`0x1432021B7`

That code passes the variable-name pointer to:

`0x1456D3A20`

alongside a string-like value/object. The surrounding routine compares several known `Env_*` names and maps successful matches to offsets in an object (`rbx + 0x38`, `+0x88`, `+0xD8`, etc.).

This is likely a name-comparison / deserialization / binding path and may be useful for understanding runtime environment payloads.

## Current working model

A descriptor appears to have a shape roughly like:

```cpp
struct EnvDescriptorCandidate {
    // flag/value metadata
    // ...
    std::uint16_t id;
    // ...
    const char* name;
    // state / auxiliary fields
};
```

The exact field offsets above are relative to the larger registry object and **not yet a final C++ struct definition**.

## Why this matters for Outlaws Tools

If the runtime registry object can be located reliably, direct variable control could give us fast and stable sliders for:

- exposure
- bloom/glare
- fog
- rain
- cloud cover
- wind
- snow/ground wetness
- lens effects
- grading parameters

without having to reverse a separate function for every control.

## Next test target

Locate the constructor/initializer entry point that owns the large descriptor object, then recover where the live instance is stored. Once found, validate one variable in-game (`Env_ExposureTarget2` is the preferred first target) by reading and changing its current value while preserving its original value for restore.
