#pragma once
#include <string>

// Options of the displaced-surface traversal ("Plan — Path tracer with
// displaced surfaces", D2, D3): the node test, the deepest level at which
// the slab is read, and the leaf intersector. Parsed with the scene (T2),
// consumed by the intersector (T4).

namespace ks
{
struct ConfigArgs;
}

namespace dmap
{

enum class BoundMode
{
    Box,  // TFDM's AABB from the min-max channel
    Slab, // the tilted region of the Taylor channels (D3)
    Both, // the intersection of the two t-intervals
};

enum class LeafMode
{
    Newton,      // Newton on the true surface P + hN
    TwoTriangle, // two triangles per texel (the validation oracle, D4)
};

struct TraversalOptions
{
    BoundMode bound = BoundMode::Both;
    // The slab is read at levels 0..slab_max_level only; above, the box
    // alone (D2: settled by T5 at 1, the leaves and 2-texel cells, where the
    // slab pays for itself on every scene measured).
    int slab_max_level = 1;
    LeafMode leaf = LeafMode::Newton;
};

bool operator==(const TraversalOptions &a, const TraversalOptions &b);
inline bool operator!=(const TraversalOptions &a, const TraversalOptions &b) { return !(a == b); }

// Keys bound ("box", "slab", "both"), slab_max_level, leaf ("newton",
// "two_triangle"); each defaults to its value in base.
TraversalOptions parse_traversal_options(const ks::ConfigArgs &args, const TraversalOptions &base);

const char *bound_mode_name(BoundMode mode);
const char *leaf_mode_name(LeafMode mode);
std::string describe(const TraversalOptions &options);

} // namespace dmap
