# Light property schema clues

Static analysis of `Outlaws.exe` found a Snowdrop schema block around `Outlaws.exe+0x1796520` that registers human-readable light/environment parameter names.

This is useful for semantics, but **must not yet be confused with the live Point/Spot instance layout**.

## Confirmed names in one registration block

Around `Outlaws.exe+0x1796520` the code repeatedly calls `Outlaws.exe+0x1758A50` with a string pointer in `r8` and a numeric value in `r9d`.

Observed mappings include:

| Name | r9d value |
|---|---:|
| `Moon` | `0x175` |
| `Diameter (Arcminutes)` | `0x06` |
| `Sprite Brightness Factor` | `0x09` |
| `Edge Sharpness` | `0x0D` |
| `Light Color` | `0x1A6` |
| `Light Intensity` | `0x07` |
| `Cloud Emission Factor` | `0x08` |
| `Override Position` | `0x186` |
| `Direction` | `0x0A` |
| `Angle` | `0x0B` |
| `Offset` | `0x0C` |
| `Texture` | `0x1D2` |

This strongly confirms separate semantic controls for color/intensity/direction/angle in Snowdrop's environment/light-related reflection/UI systems.

## Additional strings / types

- `LightIntensityUnits`
- `intensityUnits`
- `intensityScale`
- `AttenuationScale`
- `Light Color`
- `Light Intensity`

`LightIntensityUnits` has a code xref at `Outlaws.exe+0x1DD06D7` into a type/enum registration block.

## Why this matters

The Point/Spot constructors discovered in `LIGHT_FACTORY_PATHS.md` call reflected-property registration functions using numeric property IDs (for example `0x3E8`, `0x3EA`, `0x3EB`, `0x3EC`, `0x3ED`, `0x3EF`, `0x3F1`, `0x3F3`, `0x3F6`). The next goal is to connect those constructor property IDs to names/types, then trace the same properties into live world instances.

Do not use the table above as direct offsets or as proof that `r9d` is the same ID namespace as the constructor IDs; it is recorded as a semantic clue only until that relationship is proven.
