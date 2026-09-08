#include "traversal_options.h"
#include "ks/assertion.h"
#include "ks/config.h"

namespace dmap
{

namespace
{

BoundMode bound_mode_from_string(const std::string &name)
{
    if (name == "box")
        return BoundMode::Box;
    if (name == "slab")
        return BoundMode::Slab;
    if (name == "both")
        return BoundMode::Both;
    ASSERT(false, "traversal bound must be box, slab or both, got [%s]", name.c_str());
    return BoundMode::Both;
}

LeafMode leaf_mode_from_string(const std::string &name)
{
    if (name == "newton")
        return LeafMode::Newton;
    if (name == "two_triangle")
        return LeafMode::TwoTriangle;
    ASSERT(false, "traversal leaf must be newton or two_triangle, got [%s]", name.c_str());
    return LeafMode::Newton;
}

} // namespace

bool operator==(const TraversalOptions &a, const TraversalOptions &b)
{
    return a.bound == b.bound && a.slab_max_level == b.slab_max_level && a.leaf == b.leaf;
}

TraversalOptions parse_traversal_options(const ks::ConfigArgs &args, const TraversalOptions &base)
{
    TraversalOptions options = base;
    if (args.contains("bound"))
        options.bound = bound_mode_from_string(args.load_string("bound"));
    if (args.contains("slab_max_level"))
        options.slab_max_level = args.load_integer("slab_max_level");
    if (args.contains("leaf"))
        options.leaf = leaf_mode_from_string(args.load_string("leaf"));
    return options;
}

const char *bound_mode_name(BoundMode mode)
{
    switch (mode) {
    case BoundMode::Box:
        return "box";
    case BoundMode::Slab:
        return "slab";
    case BoundMode::Both:
        return "both";
    }
    return "?";
}

const char *leaf_mode_name(LeafMode mode)
{
    switch (mode) {
    case LeafMode::Newton:
        return "newton";
    case LeafMode::TwoTriangle:
        return "two_triangle";
    }
    return "?";
}

std::string describe(const TraversalOptions &options)
{
    return std::string("bound=") + bound_mode_name(options.bound) +
           " slab_max_level=" + std::to_string(options.slab_max_level) + " leaf=" + leaf_mode_name(options.leaf);
}

} // namespace dmap
