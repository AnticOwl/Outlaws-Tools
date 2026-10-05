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

This block appears to be a runtime binding/serialization layer rather than the original descriptor registry. It is significant because the slots are regular and are selected by the actual Environment variable names.

## Next steps

1. Reverse `Outlaws.exe+0x33452E0` to determine the slot type and whether it stores a live value, binding handle, or serialized wrapper.
2. Find callers of the enclosing dispatch routine to identify the owner object lifecycle.
3. Compare changes to `+0x38` / `+0x178` / `+0x268` with live Rain/Fog/Wind changes once a safe diagnostic path is available.
4. Search for equivalent runtime xrefs for `Env_ExposureTarget2`, `Env_BloomStrength2`, and `Env_LensFlareEnabled`.

No write is enabled from this mapping yet.
