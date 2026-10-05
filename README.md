# Outlaws Tools

Runtime research/tooling for **Star Wars Outlaws** (Snowdrop).

Targets:
- Light discovery/editing and light spawn/clone
- Environment: weather, rain, fog, wind, clouds, snow and Time of Day
- Post process: exposure, bloom, glare, color grading, DOF, lens flare/glare and film grain

The current revision is a safe host/scaffold. Game call targets remain unset until validated against the running game.

## Build

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```
