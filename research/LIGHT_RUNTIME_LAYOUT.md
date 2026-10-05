# Live light runtime layout findings

Static analysis of `Outlaws.exe` confirms that the Point/Spot/Tube prefab light classes have execution handlers that consume a Snowdrop node execution context rather than acting as simple constructors.

## Execution handlers

- Point: `Outlaws.exe+0x19A50D0`
- Spot: `Outlaws.exe+0x19ACE20`
- Tube: `Outlaws.exe+0x19AF2E0`

All three read node execution structures around `context+0x830` / `context+0x840` and resolve typed objects from those containers.

## Spot live renderer object

The Spot handler resolves an object through a state pointer and then loads a renderer-side object from:

```text
state + 0x38 -> renderer spot-light object
```

Confirmed writes into that renderer object include:

```text
+0x30 float
+0x34 float
+0x38 float
+0x3C float
+0x40 16-byte vector
+0x50 16-byte vector
+0x68 dword
+0xC0 dword/float payload from node parameter index 6
+0xD0 dword state/hash value
```

The handler fetches Snowdrop node parameters using numeric indices. Confirmed examples in this section include indices `6`, `7`, `0xB`, `0xC`, `0xD`, and `0xE`.

Do **not** assign semantic names (intensity/range/color/cone) to these offsets yet. They are recorded as structurally confirmed live renderer fields only.

## Tube renderer registration

The Tube handler contains an explicit registration-state bit and opposite calls:

- register/add: `Outlaws.exe+0x1C57B70`
- unregister/remove: `Outlaws.exe+0x1CB8FF0`

The Tube path passes a renderer/world owner in `rcx` and a light-side object pointer in `rdx`.

This pair is a strong lead for the renderer registration layer, but its contract must be validated before reusing it for Point/Spot.

## Prefab registration path

Light prefab nodes are registered through the common Snowdrop dispatcher:

- dispatcher: `Outlaws.exe+0x0BA6660`
- Point registration site: `+0x1DAE4EC`
- Spot registration site: `+0x1DAE536`
- Tube registration site: `+0x1DAE580`
- Area registration site: `+0x1DAE5CA`

Point and Spot additionally pass the returned node descriptor through `Outlaws.exe+0x0C79C20`. This routine finalizes node metadata; it is not a world-light spawn function.

## Current spawn strategy

Two safe reverse-engineering routes remain:

1. recover the official prefab-node execution context and invoke the native `Prefab/* Light` node correctly; or
2. recover the lower renderer/world create + register contract used beneath those node handlers.

No spawn call is enabled in the DLL until one of these contracts is proven.
