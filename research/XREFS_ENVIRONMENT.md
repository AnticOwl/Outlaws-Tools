# Environment / Time of Day xrefs

Static analysis target: supplied `Outlaws.exe` (x64 Snowdrop build).

Image base: `0x140000000`.

## Environment Control registration cluster

The environment/post-process node names are referenced from a dense registration cluster around:

- `Set Bloom` xref: `0x141DC7135`
- `Set Fog Blur` xref: `0x141DC718C`
- `Set Camera Exposure` xref: `0x141DC7274`
- `Set Fog` xref: `0x141DC7291`
- `Set Weather` xref: `0x141DC72AE`

Each registration call receives two strings:

- human-readable node path in `rdx`
- internal category/name in `rcx`

Examples:

- `Environment Control/Environment/Set Camera Exposure`
- `EnvironmentControl:Camera`

- `Environment Control/Environment/Set Fog`
- `EnvironmentControl:Fog`

- `Environment Control/Environment/Set Weather`
- `EnvironmentControl:Weather`

The call targets are small stubs that jump into the large Snowdrop code block (`.debug$P`).

### Registration stubs / real targets

- Bloom stub `0x141D0C730` -> real `0x1520CB6C0`
- Camera Exposure stub `0x141D0C830` -> real `0x1520CE950`
- Fog stub `0x141D0D030` -> real `0x1520F46E0`
- Weather stub `0x141D0D930` -> real `0x152113ED0`

These real functions share the same shape and ultimately call the common Snowdrop dispatcher/registration path at:

`0x140BA6660`

The wrapper preserves the four incoming registers and forwards them to that common function, adding a lazily initialized per-node descriptor pointer in stack parameters.

## Time of Day

`Script/Set Time Of Day` and `Script/Set Time Of Day Paused` are both registered by the same routine:

`0x157FE3EE0`

Relevant xrefs inside that routine:

- `Script/Set Time Of Day` at `0x157FE3F10`
- `Script/Set Time Of Day Paused` at `0x157FE3F70`

Both registrations also call the same common dispatcher:

`0x140BA6660`

This strongly suggests we can recover a generic Snowdrop node invocation/registration ABI and then use it for multiple tool features rather than reverse each node independently.

## Useful first-pass signatures

These are static-build signatures only and are **not yet restart/version validated**.

### Set Camera Exposure registration xref

`48 8D 15 2D 98 F9 03 48 8D 0D 5E 98 F9 03 E8 A9 55 F4 FF`

### Set Fog registration xref

`48 8D 15 68 98 F9 03 48 8D 0D 89 98 F9 03 E8 8C 5D F4 FF`

### Set Weather registration xref

`48 8D 15 8B 98 F9 03 48 8D 0D B4 98 F9 03 E8 6F 66 F4 FF`

### Set Time Of Day xref

`4C 8D 35 71 0B 1D EE 48 89 CB 8B 0D`

### Set Time Of Day Paused xref

`4C 8D 35 41 0B 1D EE 0F 8F 66 01 00 00`

## Environment variable registration evidence

The variables are not just debug strings; at least some have code xrefs in the executable.

Examples:

- `Env_ExposureTarget2` xref at `0x14178CB98`
- `Env_GameplayRainAmount` xrefs at `0x1417906A1` and `0x1432021B7`

The first registration-style xrefs look like:

`lea rax, [rip+variable_name]`

followed by storing that pointer into a larger descriptor/object at fixed offsets. This is a promising route to recover the environment variable registry and direct read/write access.

## Current interpretation

There are two viable routes for the tool:

1. **Node invocation path** — recover the generic ABI behind `0x140BA6660` and call registered Snowdrop nodes such as Set Weather / Set Fog / Set Camera Exposure / Set Bloom / Set Time Of Day.
2. **Environment registry path** — recover variable descriptors for `Env_*` values and change the backing runtime values directly.

The variable-registry route may become the fastest path for sliders. The node route is likely better for actions/presets and values that require engine-side propagation.

## Next target

Reverse `Env_ExposureTarget2` and `Env_GameplayRainAmount` registration descriptors to identify:

- type information
- current-value storage
- min/max/default metadata if present
- registry lookup function/hash

Then build a minimal in-game probe before wiring ImGui controls.
