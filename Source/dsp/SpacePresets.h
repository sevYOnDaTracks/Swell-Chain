#pragma once
#include <array>
namespace space {
inline constexpr int parameterCount=17;
inline constexpr std::array<const char*,parameterCount> ids={"timeOn","spaceOn","division","feel","feedback","delay","size","reverb","focus","tw","tl","th","sw","sl","sh","pre","tempo"};
struct SpacePreset { const char* category; const char* name; std::array<float,parameterCount> values; };
inline constexpr std::array<SpacePreset,20> presets={{
{"Vocal","Vocal Clean",{1.f,1.f,2.f,0.f,24.f,17.f,38.f,22.f,65.f,100.f,180.f,6500.f,110.f,220.f,9000.f,35.f,120.f}},
{"Vocal","Slap",{1.f,0.f,4.f,0.f,10.f,28.f,20.f,12.f,40.f,100.f,180.f,6500.f,110.f,220.f,9000.f,12.f,120.f}},
{"Vocal","Rap Throw",{1.f,1.f,1.f,0.f,48.f,32.f,45.f,20.f,80.f,100.f,180.f,6500.f,110.f,220.f,9000.f,45.f,120.f}},
{"Vocal","Wide Chorus",{1.f,1.f,3.f,1.f,35.f,25.f,62.f,35.f,60.f,100.f,180.f,6500.f,110.f,220.f,9000.f,35.f,120.f}},
{"Vocal","Dreamy",{1.f,1.f,2.f,1.f,55.f,32.f,85.f,48.f,70.f,100.f,180.f,6500.f,110.f,220.f,9000.f,55.f,120.f}},
{"Guitar","Acoustic Room",{0.f,1.f,3.f,0.f,20.f,20.f,24.f,20.f,20.f,100.f,180.f,6500.f,110.f,220.f,9000.f,18.f,120.f}},
{"Guitar","Clean Echo",{1.f,1.f,2.f,0.f,38.f,30.f,38.f,20.f,30.f,100.f,180.f,6500.f,110.f,220.f,9000.f,20.f,120.f}},
{"Guitar","Dotted Pulse",{1.f,1.f,3.f,1.f,48.f,38.f,40.f,18.f,20.f,100.f,180.f,6500.f,110.f,220.f,9000.f,25.f,120.f}},
{"Guitar","Ambient Guitar",{1.f,1.f,1.f,1.f,65.f,42.f,90.f,60.f,25.f,100.f,180.f,6500.f,110.f,220.f,9000.f,60.f,120.f}},
{"Piano","Natural Room",{0.f,1.f,2.f,0.f,20.f,15.f,30.f,22.f,15.f,100.f,180.f,6500.f,110.f,220.f,6000.f,20.f,120.f}},
{"Piano","Warm Hall",{0.f,1.f,2.f,0.f,30.f,20.f,65.f,35.f,20.f,100.f,180.f,6500.f,110.f,220.f,6000.f,30.f,120.f}},
{"Piano","Piano Echo",{1.f,1.f,2.f,0.f,32.f,24.f,45.f,27.f,35.f,100.f,180.f,6500.f,110.f,220.f,6000.f,35.f,120.f}},
{"Piano","Dreamy Keys",{1.f,1.f,1.f,2.f,52.f,35.f,85.f,50.f,30.f,100.f,180.f,6500.f,110.f,220.f,6000.f,50.f,120.f}},
{"Drums","Hi-Hat Air",{0.f,1.f,4.f,0.f,10.f,15.f,12.f,15.f,15.f,100.f,180.f,6500.f,80.f,400.f,9000.f,5.f,120.f}},
{"Drums","Snare Impact",{0.f,1.f,3.f,0.f,20.f,20.f,25.f,32.f,45.f,100.f,180.f,6500.f,80.f,400.f,9000.f,28.f,120.f}},
{"Drums","Snare Plate",{0.f,1.f,3.f,0.f,20.f,20.f,45.f,35.f,30.f,100.f,180.f,6500.f,80.f,400.f,9000.f,25.f,120.f}},
{"Drums","Percussion Room",{0.f,1.f,3.f,0.f,20.f,20.f,22.f,25.f,20.f,100.f,180.f,6500.f,80.f,400.f,9000.f,12.f,120.f}},
{"Drums","Drum Ambience",{0.f,1.f,2.f,0.f,20.f,20.f,18.f,18.f,35.f,100.f,180.f,6500.f,80.f,400.f,9000.f,15.f,120.f}},
{"Creative","Space Drift",{1.f,1.f,1.f,2.f,68.f,45.f,80.f,50.f,40.f,100.f,180.f,6500.f,110.f,220.f,9000.f,65.f,120.f}},
{"Creative","Infinite Clouds",{1.f,1.f,0.f,1.f,85.f,50.f,100.f,70.f,20.f,100.f,180.f,6500.f,110.f,220.f,9000.f,90.f,120.f}}
}};} // namespace space
