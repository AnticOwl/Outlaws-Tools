# Outlaws.exe static findings

Source: supplied `Outlaws.exe`, PE32+ x64, Snowdrop traces present.

## High-confidence environment control strings

- `Environment Control/Environment/Set Weather`
- `Environment Control/Environment/Set Fog`
- `Environment Control/Environment/Set Camera Exposure`
- `Environment Control/Post-Effects/Set Bloom`
- `Environment Control/Post-Effects/Set Glare`
- `Environment Control/Post-Effects/Set Color Grading: Gameplay`
- `Environment Control/Post-Effects/Set Color Grading: Indoor`
- `Environment Control/Post-Effects/Set Color Grading: Outdoor`
- `Environment Control/Post-Effects/Set Depth Of Field`
- `Environment Control/Post-Effects/Set Fog Blur`
- `Script/Environment/Set Environment Preset`
- `Script/Environment/Remove Environment Preset`
- `Script/Set Time Of Day`
- `Script/Set Time Of Day Paused`
- `Script/Get Time Of Day`
- `HC_SetTimeOfDayNode`
- `HC_SetTimeOfDayPausedNode`
- `HC_GetTimeOfDayNode`
- `HC_EnvironmentNodeGetWeatherPreset`

## Environment variables found

### Weather
- `Env_GameplayRainAmount`
- `Env_GameplayFogAmount`
- `Env_GameplayWindStrength`
- `Env_GameplayCloudCover`
- `Env_RainGroundAmount`
- `Env_SnowGroundAmount`
- `Env_IsSnowing`

### Fog
- `Env_OutdoorFogDensity`
- `Env_OutdoorFogColor`
- `Env_OutdoorFogScaleHeight`
- `Env_IndoorFogDensity`
- `Env_GroundFogAlpha`
- `Env_GroundFogHeight`
- `Env_FogNoiseStrength`

### Sun / moon / ToD
- `Environment.TimeOfDay`
- `Env_SunDirection`
- `Env_SunIntensity2`
- `Env_SunColor2`
- `Env_MoonDirection`
- `Env_MoonLightIntensity2`
- `Env_OverrideSunPosition`
- `Env_OverrideMoonPosition`

### Clouds
- `Env_VolCloudCoverage`
- `Env_VolCloudsDensity`
- `Env_CloudCoverageForced`
- `Env_CloudStrength`

## Post process variables found

### Exposure
- `Env_ExposureTarget2`
- `Env_ExposureTargetLerp2`
- `Env_AutoExposurePupilMin`
- `Env_AutoExposurePupilMax`
- `Env_AutoExposurePupilOffset`
- `Env_LocalExposureBrightnessCutoff`
- grading exposure variables for shadow/midtone/highlight/global ranges

### Bloom / glare
- `Env_BloomStrength2`
- `Env_BloomStrengthIndoor`
- `Env_BloomDirtAlpha`
- `Env_GlareEnabled`
- `Env_GlareStrength`
- `Env_GlareThreshold`

### Lens
- `Env_LensFlareEnabled`
- `Env_LensFlareOpacity`
- `Env_LensGlareEnabled`
- `Env_LensDistortionEnabled`
- `Env_LensFringeEnabled`
- `Env_CameraLensOpticsEnabled`

### Film grain
- `Env_FilmGrainEnabled`
- `Env_FilmGrainAmount`

## Light-related strings found

- `IESLightProfile`
- `LightImpostor`
- `HC_LightImpostor`
- `WorldTest_LightImpostor`
- `lightImpostorUID`
- spotlight/light texture references are present in the executable.

## Next reverse-engineering milestones

1. Xref the environment-control strings to recover node descriptors/registrations and their invoke functions.
2. Recover the environment variable registry/hash lookup path for direct runtime reads/writes.
3. Xref `HC_SetTimeOfDayNode` and `HC_ScriptEnvironmentSetWeatherNodePinData`.
4. Identify the light world registry/component type and enumerate live lights.
5. Find light create/clone/destroy calls; prefer cloning an existing light as first spawn validation.
6. Add signatures/AOBs only after live validation across a restart.
