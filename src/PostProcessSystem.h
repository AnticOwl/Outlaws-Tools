#pragma once

namespace outlaws {
struct PostProcessState {
    float exposure{1.f};
    float bloom{1.f};
    float glare{1.f};
    float saturation{1.f};
    float contrast{1.f};
    float gamma{1.f};
    bool lensFlare{true};
    bool lensGlare{true};
    bool depthOfField{true};
    bool filmGrain{true};
};

class PostProcessSystem {
public:
    bool read(PostProcessState& out) const;
    bool apply(const PostProcessState& state);
};
}
