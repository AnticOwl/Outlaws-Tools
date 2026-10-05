# Light-related property clues

Static analysis of `Outlaws.exe` found a Snowdrop schema block around `Outlaws.exe+0x1796520` that contains `Light Color` and `Light Intensity`.

**Correction:** this block is the **Moon environment schema**, not the live Point/Spot Light schema. It is useful for the Environment / Time-of-Day tool, but must not be used to name Point/Spot renderer offsets.

## Confirmed Moon/environment mappings

Around `Outlaws.exe+0x1796520` the code repeatedly calls `Outlaws.exe+0x1758A50` with a string pointer in `r8` and a numeric value in `r9d`.

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

Immediately after this block the same registration function begins the camera-exposure schema (`Exposure Target`, `Exposure Target Lerp`, `Auto Exposure Min/Max/Offset`, etc.), further confirming that `Light Color`/`Light Intensity` here belong to Environment/ToD.

## Still-relevant light type information

- `LightIntensityUnits`
- `intensityUnits`
- `intensityScale`
- `AttenuationScale`

`LightIntensityUnits` has a code xref at `Outlaws.exe+0x1DD06D7` into a type/enum registration block. That remains a valid lead for actual light units.

## Point / Spot property IDs

The Point/Spot schema constructors register numeric reflected-property IDs (for example `0x3E8`, `0x3EA`, `0x3EB`, `0x3EC`, `0x3ED`, `0x3EF`, `0x3F1`, `0x3F3`, `0x3F6`). Their human-readable names are **not yet proven**.

Do not map the Moon table above onto those IDs. The next goal remains tracing the Point/Spot IDs or runtime parameter indices into live renderer fields.
