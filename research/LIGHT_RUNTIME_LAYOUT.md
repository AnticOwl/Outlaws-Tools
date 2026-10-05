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
+0x60 dynamic array pointer
+0x68 dynamic array count
+0xC0 payload from Snowdrop parameter index 6
+0xD0 renderer-state/hash value
```

The handler fetches Snowdrop node parameters using numeric indices. Confirmed examples in this section include indices `6`, `7`, `8`, `9`, `0xA`, `0xB`, `0xC`, `0xD`, `0xE`, `0xF`, `0x11`, and `0x12`.

Do **not** assign semantic names (intensity/range/color/cone) to these offsets yet. They are recorded as structurally confirmed live renderer fields only.

## Generic renderer object creation / registration

A major shared renderer path is now confirmed:

- wrapper / manager entry: `Outlaws.exe+0x1B70C20`
- existing-manager create path: `Outlaws.exe+0x1B70650`

`0x1B70C20` is called from the Spot handler and from multiple unrelated renderer systems. It returns a 32-bit handle. The caller stores these handles in arrays, confirming this is a generic renderer registration/create mechanism rather than Spot-only logic.

Observed Spot call contract at `Outlaws.exe+0x19ADCAF`:

```text
rcx = renderer/world manager object (resolved from world + 0x8E68)
rdx = 0x40-byte transform/matrix payload
r8d = renderer payload at spotObject+0xC0
r9  = renderer resource/object pointer
stack +0x20 = pointer to spotObject+0x30 parameter block
stack +0x28 = secondary owner/resource pointer
stack +0x30 = optional pointer (zero in this Spot path)
return eax = renderer handle
```

The wrapper checks `manager+0x38`. If a live internal manager already exists it calls `0x1B70650`; otherwise it builds/copies the registration data and inserts it into the manager before returning the new handle.

### Existing-manager path details

`Outlaws.exe+0x1B70650`:

- allocates/reuses a numeric handle/index from the manager;
- stores a renderer entry in a stride of `0xA0` bytes under `manager+0x30`;
- copies a 0x40-byte transform payload into entry `+0x00..+0x3F`;
- stores a retained object/resource pointer at entry `+0x40`;
- copies additional parameter data to `+0x50..+0x7F`;
- computes/stores a hash/state at `+0x80`;
- stores an optional retained pointer at `+0x88`;
- updates a smaller per-handle table under `manager+0x20`;
- returns the allocated handle in `eax`.

This is currently the strongest candidate for the common lower-level renderer registration layer needed by Point/Spot spawn.

## Other confirmed call sites for the generic renderer creator

Direct call sites to `0x1B70C20` were found at:

```text
Outlaws.exe+0x07CF6B5
Outlaws.exe+0x19ADCAF  // Spot prefab handler
Outlaws.exe+0x1F06CF1
Outlaws.exe+0x31FE852
Outlaws.exe+0x31FE983
```

The non-Spot callers use the same broad argument shape (transform payload + parameter block + owner/resource pointers), reinforcing that `0x1B70C20` is a generic renderer-object creation/registration API.

## Tube renderer registration

The Tube handler contains an explicit registration-state bit and opposite calls:

- register/add: `Outlaws.exe+0x1C57B70`
- unregister/remove: `Outlaws.exe+0x1CB8FF0`

The Tube path passes a renderer/world owner in `rcx` and a light-side object pointer in `rdx`.

This pair is a strong lead for a second renderer registration layer, but its contract must be validated before reusing it for Point/Spot.

## Prefab registration path

Light prefab nodes are registered through the common Snowdrop dispatcher:

- dispatcher: `Outlaws.exe+0x0BA6660`
- Point registration site: `+0x1DAE4EC`
- Spot registration site: `+0x1DAE536`
- Tube registration site: `+0x1DAE580`
- Area registration site: `+0x1DAE5CA`

Point and Spot additionally pass the returned node descriptor through `Outlaws.exe+0x0C79C20`. This routine finalizes node metadata; it is not a world-light spawn function.

## Current spawn strategy

The preferred route is now:

1. recover a valid live renderer/world manager pointer used by `0x1B70C20`;
2. recover the exact Spot parameter/resource objects passed at `r9` and stack arguments;
3. clone an existing live Spot registration payload and call the generic renderer creator;
4. only after that succeeds, synthesize Point/Spot payloads from scratch.

This clone-first route is safer than fabricating undocumented resource objects and should give the first visible spawned light with the least risk.

No spawn call is enabled in the DLL until the remaining owner/resource contract is proven live.
