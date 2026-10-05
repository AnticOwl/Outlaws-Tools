# Light / Prefab xrefs

Static analysis target: supplied `Outlaws.exe` (x64 Snowdrop build).

## Prefab light registrations

A dense prefab-registration block exists around `0x141DAE450` and uses the same common Snowdrop registration path seen for Environment Control nodes:

`0x140BA6660`

Confirmed entries:

- `Prefab/Point Light` at xref `0x141DAE4EC`
- internal name `prefab:Light` at `0x145D61438`
- registration call at `0x141DAE51A`
- returned descriptor is immediately passed to `0x140C79C20`

- `Prefab/Spot Light` at xref `0x141DAE536`
- internal name `prefab:SpotLight` at `0x145D61460`
- registration call at `0x141DAE564`
- returned descriptor is immediately passed to `0x140C79C20`

Adjacent supported prefab types are also present:

- `Prefab/Tube Light` / `prefab:TubeLight`
- `Prefab/Area Light` / `prefab:AreaLight`
- another `Prefab/Light` / `prefab:Light2`

This is strong evidence that the engine exposes prefab definitions for multiple light types, not only renderer-internal light proxies.

## Graphics light type registration

Renderer-side type registration is also visible:

- `gfx:PointLight` xref at `0x151CCE4C5`
- the type-name registration call is `0x140D11A40`

- `gfx:SpotLight` xref at `0x151CD4D75`
- the same type-name registration call is `0x140D11A40`

The returned/static type objects are stored around:

- PointLight: `0x149668800`
- SpotLight: `0x149668820`

These addresses are static-build observations only and must not be treated as version-stable pointers.

## Interpretation

There are at least two useful layers:

1. **Prefab layer** (`Prefab/Point Light`, `Prefab/Spot Light`, `Prefab/Tube Light`, `Prefab/Area Light`) — likely the best route to actual entity/prefab spawning.
2. **gfx layer** (`gfx:PointLight`, `gfx:SpotLight`) — useful for identifying renderer light structures and enumerating/editing live light data.

For the tool, the preferred order is:

1. recover the prefab descriptor returned for Point/Spot Light;
2. determine what `0x140C79C20` does with the descriptor after registration;
3. find callers that instantiate a registered prefab type;
4. separately use `gfx:PointLight` / `gfx:SpotLight` type objects to discover live renderer-side instances and editable properties.

## First-pass xref signatures

Static build only, not yet restart/version validated.

### Point Light prefab string xref

`4C 8D 2D 2D 2F FB 03 0F 8F 12 18 00 00`

### Spot Light prefab strings

`4C 8D 25 30 2F FB 03 39 05 6A F4 91 07 4C 8D 2D 0B 2F FB 03`

## Next target

Reverse `0x140C79C20` and find the spawn/instantiate path consuming registered prefab descriptors. In parallel, follow the PointLight/SpotLight type objects to identify live light containers and fields such as transform, intensity, range, color, cone angles and shadow flags.
