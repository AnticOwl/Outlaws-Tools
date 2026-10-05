# Runtime Environment name lookup path

Static analysis found a second code xref for `Env_GameplayRainAmount` at `Outlaws.exe+0x32021B7`, separate from the descriptor constructor. This routine performs a runtime string-name dispatch over several Environment variables.

The comparison helper is called repeatedly with a candidate string and a literal `Env_*` name. On match, the routine selects a fixed slot in a destination object and passes it to `Outlaws.exe+0x33452E0`.

## Confirmed mappings in this dispatch block

| Environment name | Destination slot from `rbx` |
|---|---:|
| `Env_IsSnowing` | flag at `+0x2B8` |
| `Env_IsFoggy` | flag at `+0x2B9` |
| `Env_GameplayRainAmount` | `+0x38` |
| `Env_RainGroundAmount` | `+0x88` |
| `Env_Temperature` | `+0xD8` |
| `Env_ViewDistance` | `+0x128` |
| `Env_OutdoorFogDensity` | `+0x178` |
| `Env_CloudCoverageScale` | `+0x1C8` |
| `Env_WindDirection` | `+0x218` |
| `Env_WindStrength` | `+0x268` |

The matched slot is passed to:

```text
Outlaws.exe+0x33452E0
```

## Correct subsystem identification

The owner of this dispatch is not the visual Environment renderer. Its function table is immediately followed by audio/RTPC identifiers including:

- `TimeOfDayRTPC`
- `WeatherPrecipitationIntensityRTPC`
- `WeatherWindIntensityRTPC`
- `WeatherTemperatureRTPC`
- `WeatherViewDistanceRTPC`

This identifies the block as a **weather audio/RTPC binding layer**. The fixed slots above therefore represent audio-side bindings derived from `Env_*` variables rather than the authoritative visual Environment values themselves.

`Outlaws.exe+0x33452E0` converts a Snowdrop variant/value into the typed binding slot used by this RTPC object.

## Why it still matters

This confirms that `Env_*` names are consumed at runtime outside the descriptor constructor and gives us one concrete example of how Snowdrop resolves a named Environment variable into a typed consumer. It is useful for understanding the variable plumbing, but it should not be used as the visual Environment write path.

## Next steps

1. Keep the descriptor registry path for authoritative `Env_*` metadata.
2. Search for additional runtime xrefs for `Env_ExposureTarget2`, `Env_BloomStrength2`, `Env_LensFlareEnabled`, and other visual-only variables; those should lead to renderer/post-process consumers rather than RTPC consumers.
3. Identify visual consumers of `Env_OutdoorFogDensity` and `Env_GameplayRainAmount` that are distinct from this audio layer.
4. Do not write to the RTPC binding object when implementing Environment sliders.

No write is enabled from this mapping.
