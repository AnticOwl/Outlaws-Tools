# Snowdrop light type tables

Static reverse-engineering notes for the supplied `Outlaws.exe` (image base `0x140000000`).

## Registered prefab type globals

| Type | Global RVA | Simple getter RVA | Notes |
|---|---:|---:|---|
| Point Light | `0x9666900` | `0x19C7C90` | getter returns the registered Point Light descriptor pointer |
| Spot Light | `0x9666A50` | `0x19C7DE0` | getter returns the registered Spot Light descriptor pointer |
| Tube Light | `0x9666908` | `0x19C7E50` | getter returns the registered Tube Light descriptor pointer |
| Base Light | `0x96669D0` | `0x19C7C80` | base light descriptor |
| Area Light | `0x96BE680` | not yet confirmed | registered by the same prefab block but no simple getter confirmed yet |

The registration block contains the strings:

- `Prefab/Point Light` / `prefab:Light`
- `Prefab/Spot Light` / `prefab:SpotLight`
- `Prefab/Tube Light`
- `Prefab/Area Light`
- `Prefab/Light`

## Method tables

The simple getter functions also appear as entries in larger Snowdrop method/type tables:

- Point table region: around `0x145CCB7C0`
- Spot table region: around `0x145CD3530`
- Tube table region: around `0x145CCBA50`

The tables share many common engine slots and contain several type-specific functions.

Important correction: the first obvious Point/Spot specific functions near `0x1419706D0` and `0x141970FA0` are destructor/free paths (they accept the usual delete flags and call the engine allocator), not spawn functions. They must not be used as factories.

Other type-specific slots around `0x1419A50D0`, `0x1419ACE20`, and `0x1419AF2E0` contain substantial runtime/update logic and are being used to map the live light component layout.

## Current direction for spawn

Do not call the registration helper (`0x140BA6660`) or descriptor finalizer (`0x140C79C20`) as a spawner. They register/configure prefab descriptors only.

Next target is the actual object/prefab factory that consumes one of the registered light type descriptors and allocates/attaches the live component. The safest first live validation remains cloning/instantiating through the same factory used by existing Point/Spot prefab instances.
