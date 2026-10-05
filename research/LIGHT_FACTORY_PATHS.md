# Snowdrop light factory paths

Static analysis of the supplied `Outlaws.exe` (image base `0x140000000`).

## Confirmed object allocation wrappers

These wrappers allocate an object and tail-jump to the matching constructor. They are **not yet treated as world-spawn calls**; they create the type/object, while registration/attachment to the active world still needs to be recovered.

### Point Light
- Factory wrapper RVA: `0x1D4E090`
- Allocation size: `0x38`
- Constructor RVA: `0x194E220`
- Runtime type global RVA: `0x9666900`
- Getter RVA: `0x19C7C90`
- Type/vtable reference installed by constructor: `0x5CCB808`

Factory sequence:
```asm
mov ecx, 38h
call Outlaws.exe+0B17900   ; allocator
...
jmp Outlaws.exe+194E220   ; Point ctor
```

### Spot Light
- Factory wrapper RVA: `0x1D4E4C0`
- Allocation size: `0x40`
- Constructor RVA: `0x1950560`
- Runtime type global RVA: `0x9666A50`
- Getter RVA: `0x19C7DE0`
- Type/vtable reference installed by constructor: `0x5CD3578`

Factory sequence:
```asm
mov ecx, 40h
call Outlaws.exe+0B17900
...
jmp Outlaws.exe+1950560
```

### Tube Light
- Factory wrapper RVA: `0x1D4E5E0`
- Allocation size: `0x40`
- Constructor RVA: `0x1951360`
- Runtime type global RVA: `0x9666908`
- Getter RVA: `0x19C7E50`
- Type/vtable reference installed by constructor: `0x5CCBA98`

Factory sequence:
```asm
mov ecx, 40h
call Outlaws.exe+0B17900
...
jmp Outlaws.exe+1951360
```

## Runtime type registration

The globals above are populated in the Snowdrop registration block around `Outlaws.exe+0x1DAFDxx`.

Point registration is visible around `+0x1DAFD2B` / `+0x1DAFD60`:
- descriptor/table `0x5D59048`
- runtime descriptor storage around `0x96CD8F0`
- global `0x9666900` receives the runtime descriptor pointer

Spot registration is visible around `+0x1DAFDA0` / `+0x1DAFDD2`:
- descriptor/table `0x5D5A810`
- runtime descriptor storage around `0x96CD950`
- global `0x9666A50` receives the runtime descriptor pointer

Tube registration is visible around `+0x1DAFE12` / `+0x1DAFE44`:
- descriptor/table `0x5D590A8`
- runtime descriptor storage around `0x96CD9B0`
- global `0x9666908` receives the runtime descriptor pointer

## Important interpretation

The factory wrappers above are strong evidence for construction of the light class/type objects, but **calling them alone is not yet equivalent to `Spawn Light` in the world**. The missing piece is the world/entity/component registration path that owns the lifetime, transform and renderer registration of a live light.

## Constructor observations

Point and Spot constructors initialize common fields:
- byte at `+0x28` = `0`
- float at `+0x2C` = `1.0`
- float at `+0x30` = `1.0`

Spot additionally initializes qword `+0x38` = `0`.

The constructors then register/reflection-bind a series of property IDs via the virtual slot at `+0x250`. Those IDs are a promising path for recovering semantic fields such as intensity/range/color/cone angles.

## Next

1. Recover the property-name mapping for the Point/Spot reflection IDs used by the constructors.
2. Find code that consumes the constructed object and registers it into a world/entity/component manager.
3. Identify live instance layout for transform, color, intensity, range and spot angles.
4. Only then expose `spawnPoint()` / `spawnSpot()` as active calls in the tool.
