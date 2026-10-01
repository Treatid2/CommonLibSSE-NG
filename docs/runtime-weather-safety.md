# Weather bytes and guarded menu views

This correction is based on the complete v10.1.0 release at
`39f9d07a6ffabea8fb559eee87ab7d27cd463e8a`. It selectively ports weather
and menu safety from ParticleTroned's
`73b2384f7691dd04ab3d80d1fc9760bc8ae97811`; it preserves the newer release's
device-aware `Inventory3DManager::ToggleItemZoom(VR_DEVICE)` and modern Havok
definitions instead of replacing the dependency with the older backport.

Weather fields used as 0–255 settings are unsigned bytes. Rain and snow
use normalized fade thresholds with strict comparisons: equality does not
start current precipitation or retain previous precipitation. Current and
previous weather flags are evaluated independently, including absent weather.
These types also govern direct editor, picker and wetness consumers.

`RaceSexMenu::GetRuntimeData()` and `Inventory3DManager::GetRuntimeData()`
now return nullable pointers to the flat layout. They reject VR. Their
`GetVRRuntimeData()` counterparts reject SE/AE. Check the returned pointer
and use arrow access. These are source API changes; they do not change the
engine layouts. The VR menu omits the flat camera, and the inventory accessor
exposes only the independently confirmed VR array prefix.

The registered unit source covers every stored precipitation byte, strict
and adjacent float boundaries, mixed weather, unsigned direct-field round
trips, const/mutable runtime rejection and native offsets. Compilation across
SE, AE, VR, FLATRIM and ALL, native assertion execution, CSX compilation and
live-engine qualification are separate validation gates. Source assertions
and mocked views do not prove fixed-slot dispatch in a running engine.
