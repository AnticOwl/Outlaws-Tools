# Environment re-analysis (Oct 2026)

This note supersedes earlier assumptions about the Environment descriptor table and the so-called runtime lookup path.

## 1. Descriptor registry: corrected interpretation

The authoritative Environment descriptor table is static and begins at:

```text
Outlaws.exe + 0x9658C70
count = 0x1E8
stride = 0x40
```

ID lookup helper:

```text
Outlaws.exe + 0x177A410
CX = Env ID
RAX = descriptor pointer or 0
```

The helper computes:

```text
descriptor = base + id * 0x40
```

### Corrected descriptor layout

Static constructor analysis at `Outlaws.exe+0x178A240` shows:

```text
+0x00  uint32 metadata/flags
+0x04  uint16 id
+0x06  padding
+0x08  const char* name
+0x10  default/value metadata (type-dependent payload)
+0x18  metadata
+0x1C  metadata byte
+0x20  typeInfo / type descriptor pointer
+0x28  range/constraint flag
+0x2C  float constraint value A
+0x30  range/constraint flag
+0x34  float constraint value B
+0x38  metadata/constraint flag
```

The earlier layout that placed `name` at `+0x38` was wrong.

## 2. Critical correction: 0x17664B0 is NOT a live-value getter

`Outlaws.exe+0x17664B0` is a thunk to:

```text
0x151899B20
```

That function linearly scans the static descriptor array by comparing the input string with each descriptor name.

It returns the matching **descriptor pointer**, not a live Environment value.

This proves that earlier addresses such as:

```text
Env_FilmGrainAmount record = moduleBase + 0x965C6B0
```

were descriptors:

```text
0x9658C70 + 0xE9 * 0x40 = 0x965C6B0
```

Therefore previous direct writes to `record+0x10` changed descriptor metadata/default payload, not renderer/runtime state.

This likely explains instability/crashes in the Post and Weather direct-write tests.

## 3. Time of Day: verified native path

The TOD path is separate from the Env descriptor table and is confirmed working in-game.

Nodes:

```text
Script/Set Time Of Day
HC_SetTimeOfDayNode

Script/Set Time Of Day Paused
HC_SetTimeOfDayPausedNode

Script/Is Time Of Day Paused
HC_IsTimeOfDayPausedNode

Script/Get Time Of Day
HC_GetTimeOfDayNode
```

Registration block:

```text
Outlaws.exe + 0x17FE3EE0
```

Native setter:

```text
Outlaws.exe + 0x32F1250
RCX = TimeOfDaySystem*
EDX = hour
R8D = minute
R9D = second
```

The function converts H:M:S into seconds, then milliseconds, and stores time at:

```text
TimeOfDaySystem + 0x18
```

Pause:

```text
TimeOfDaySystem + 0x22
```

The current resolver used by the project has been validated in-game.

## 4. Environment command setters: still valid, but queue timing matters

Float command setter:

```text
Outlaws.exe + 0x17D9C20
RCX = EnvironmentSystem*
DX  = Env ID
XMM2 = float value
R9D = flags
stack argument = extra/context
```

Bool command setter:

```text
Outlaws.exe + 0x17D9CF0
RCX = EnvironmentSystem*
DX  = Env ID
R8B = bool
R9D = flags
stack argument = extra/context
```

These functions do not directly write renderer state. They:

1. resolve a Snowdrop thread slot using `Outlaws.exe+0xCD6BE0`;
2. select a lane at `EnvironmentSystem + slot * 0x10`;
3. allocate a command object;
4. store Env ID / flags / payload;
5. append the command pointer to that thread-local lane.

Float command object observations:

```text
size = 0x20
+0x0C = Env ID
+0x10 = flags
+0x18 = float payload
```

## 5. Environment update / queue merge

`Outlaws.exe+0x17B9CA0` is not the actual value setter. It merges queued commands.

It scans all per-thread lanes from the EnvironmentSystem base up to approximately `+0x4000` in `0x10` byte increments and moves queued command pointers into a central list.

Important consequence for earlier tests:

The previous hook did:

```text
original Environment update
then enqueue our command
```

That means our command was appended **after** the native merge pass.

For the next test, commands should be queued before the native merge/update pass, then the same lane should be checked after the original function returns.

This may explain why previous queued writes appeared to remain pending.

## 6. Gameplay Weather Env IDs and descriptor ranges

Static constructor analysis confirms:

```text
0xD0 Env_GameplayRainAmount
0xD1 Env_GameplayFogAmount
0xD2 Env_GameplayCloudCover
0xD3 Env_GameplayWindStrength
0xD4 Env_LightningAmount
```

For these descriptors the static constraints are consistent with a normalized range:

```text
min = 0.0
max = 1.0
```

So for `Env_GameplayRainAmount`, the safe/extreme test value is **1.0**, not 5.0 or 10.0.

The earlier plan to probe values above 1.0 should be discarded.

## 7. Weather preset data is a richer structure than one Env float

Relevant symbols:

```text
World/Get Weather Preset
HC_EnvironmentNodeGetWeatherPreset

HC_ScriptEnvironmentSetWeatherNodePinData
Script/Environment/Set Environment Preset
Script:EnvironmentSetWeatherPreset
Script/Environment/Set Removable Environment Preset
Script:EnvironmentSetRemovableWeatherPreset
Script/Environment/Remove Environment Preset
Script:EnvironmentRemoveWeatherPreset
```

Serialized WeatherPreset constant-data members reveal a richer weather object:

```text
myGameplayRainAmount
myGraphicsRainAmount
myTemperature
myViewDistance
myOutdoorFog
myCloudCoverage
myWindDirection
myWindStrength
myHasSnow
myHasFog
```

Observed structure offsets from serializer/deserializer code:

```text
+0x038  myGameplayRainAmount
+0x080  validity/presence flag

+0x088  myGraphicsRainAmount
+0x0D0  validity/presence flag

+0x0D8  myTemperature
+0x120  validity/presence flag

+0x128  myViewDistance
+0x170  validity/presence flag

+0x178  myOutdoorFog
+0x1C0  validity/presence flag

+0x1C8  myCloudCoverage
+0x210  validity/presence flag

+0x218  myWindDirection
+0x260  validity/presence flag

+0x268  myWindStrength
+0x2B0  validity/presence flag

+0x2B8  myHasSnow
+0x2B9  myHasFog
```

This strongly suggests that visual weather is driven by presets/blended weather state, not just by a single descriptor value.

## 8. Environment Control registrations

The visual/editor-like EnvironmentControl family is registered around:

```text
Outlaws.exe + 0x1DC70xx
```

Important registrations:

```text
Environment Control/Post-Effects/Set Bloom
EnvironmentControl:Bloom

Environment Control/Post-Effects/Set Glare
EnvironmentControl:Glare

Environment Control/Post-Effects/Set Depth Of Field
EnvironmentControl:DOF

Environment Control/Environment/Set Camera Exposure
EnvironmentControl:Camera

Environment Control/Environment/Set Fog
EnvironmentControl:Fog

Environment Control/Environment/Set Weather
EnvironmentControl:Weather

EnvironmentControl:Clouds
EnvironmentControl:Sky
EnvironmentControl:Misc
EnvironmentControl:SetValue
```

Set Weather registration:

```text
Outlaws.exe + 0x1DC72AE
builder stub     = Outlaws.exe + 0x1D0D930
builder target   = 0x152113ED0
```

These are registration/build functions, not direct setters.

## 9. Script Weather registrations

Script Environment registrations are separate and located around:

```text
Outlaws.exe + 0x447B07F
```

They include:

```text
Script/Environment/Set Environment Preset
Script/Environment/Set Removable Environment Preset
Script/Environment/Remove Environment Preset
Script/Environment/Weather Loaded From Save Sucessfully
```

This is a separate path from `EnvironmentControl:Weather`.

The project should keep these two systems distinct:

- Script Environment preset system = gameplay/preset orchestration
- EnvironmentControl system = visual/editor-style controls

## 10. Audio/RTPC weather block remains a false lead for visual control

The previously identified dispatch that maps:

```text
Env_GameplayRainAmount
Env_OutdoorFogDensity
Env_WindStrength
...
```

into fixed object offsets is an audio/RTPC binding layer.

Nearby identifiers include:

```text
TimeOfDayRTPC
WeatherPrecipitationIntensityRTPC
WeatherWindIntensityRTPC
WeatherTemperatureRTPC
WeatherViewDistanceRTPC
```

Do not use that object as the visual Environment source.

## 11. Why run 70 crashed

Run 70 called Rain logging immediately during DLL bootstrap.

That code used `0x17664B0` under the false assumption that it returned a live runtime record, then parsed descriptor memory using an incorrect descriptor layout.

Even if the descriptor lookup itself is safe, the subsequent metadata interpretation was wrong and the code path was introduced at injection time.

For tomorrow:

- do not use run 70;
- start from the last known stable TOD-only build;
- remove all direct descriptor writes;
- remove the fake runtime-value abstraction;
- use descriptors only for ID/name/type/range metadata.

## 12. Recommended next test order

1. Keep verified TOD code unchanged.
2. Restore stable Environment-only bootstrap.
3. Add Rain test through the native Environment command setter.
4. Queue command **before** `+0x17B9CA0` performs its merge.
5. Use ID `0xD0`, values `0.0` and `1.0`.
6. Log:
   - descriptor ID/name/range only;
   - thread lane count before enqueue;
   - after enqueue;
   - after native merge returns;
   - central queue count if its layout can be safely confirmed.
7. Observe Rain visually.
8. If no visual result, move to the native WeatherPreset / EnvironmentControl:Weather execution path rather than writing metadata.
9. Then test Fog `0xD1`, CloudCover `0xD2`, WindStrength `0xD3`, LightningAmount `0xD4`.
10. Only after Weather is understood should Post be revisited using the corrected queue timing.

## 13. Symbol inventory

The executable contains no embedded COFF symbol table. Useful "symbols" are Snowdrop strings / RTTI-style names / node class names.

A scan found over 500 Environment-related names, including complete groups for:

- atmosphere
- clouds
- rain/snow/dust
- fog/ground fog
- sun/moon/sky
- TOD/geographic coordinates
- wind/gust
- exposure/local exposure
- bloom/glare/lens optics
- DOF
- grading
- GI
- weather presets

The most useful symbols for further reverse are:

```text
HC_EnvironmentNodeGetWeatherPreset
HC_ScriptEnvironmentSetWeatherNodePinData
HC_EnvironmentWeatherPresetConstantData*
HC_SetTimeOfDayNode
HC_SetTimeOfDayPausedNode
HC_GetTimeOfDayNode
HC_IsTimeOfDayPausedNode
EnvironmentControl:Weather
EnvironmentControl:Fog
EnvironmentControl:Clouds
EnvironmentControl:Sky
EnvironmentControl:SetValue
```
