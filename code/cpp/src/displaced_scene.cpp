#include "displaced_scene.h"
#include "ks/assertion.h"
#include "ks/log_util.h"

using namespace ks;

namespace dmap
{

int DisplacedScene::displaced_of(uint32_t subscene, uint32_t geometry) const
{
    auto it = displaced_index.find({subscene, geometry});
    return it == displaced_index.end() ? -1 : it->second;
}

int DisplacedScene::shape_of(uint32_t instance, uint32_t geometry) const
{
    auto it = shape_index.find({instance, geometry});
    return it == shape_index.end() ? -1 : it->second;
}

namespace
{

struct Builder
{
    const SceneSpec &spec;
    const EmbreeDevice &device;
    DisplacedScene &out;
    std::vector<int> displaced_of_shape; // the DisplacedObject of a shape, or -1

    // The geometry and material of one shape; a displaced shape gets the
    // walk or its substitute, and its own material.
    void add_shape(SubScene &sub, int shape_index)
    {
        const SceneShape &shape = spec.shapes[shape_index];
        int di = displaced_of_shape[shape_index];
        const Material *material = shape.material;
        if (di >= 0) {
            const DisplacedObject &d = spec.displaced[di];
            material = d.material;
            if (out.substitute_subdivision > 0) {
                std::vector<TexelFace> faces;
                out.substitutes[di] =
                    std::make_unique<MeshData>(spec.texel_tessellated(d, out.substitute_subdivision, faces));
                out.substitutes[di]->twosided = shape.mesh->twosided;
                out.substitute_faces[di] = std::move(faces);
                sub.geometries.push_back(std::make_unique<MeshGeometry>(*out.substitutes[di]));
            } else {
                auto geom = std::make_unique<DisplacedGeometry>(d, shape.mesh->twosided);
                out.displaced[di] = geom.get();
                sub.geometries.push_back(std::move(geom));
            }
        } else {
            sub.geometries.push_back(std::make_unique<MeshGeometry>(*shape.mesh));
        }
        ASSERT(material, "shape [%s] has no material", shape.name.c_str());
        sub.materials.push_back(material);
        out.shapes[shape_index].geometry = (int)sub.geometries.size() - 1;
    }

    int add_subscene(const std::vector<int> &shape_indices)
    {
        SubScene sub;
        for (int s : shape_indices)
            add_shape(sub, s);
        sub.create_rtc_scene(device);
        int id = (int)out.scene.subscenes.size();
        out.scene.add_subscene(std::move(sub));
        for (int s : shape_indices) {
            out.shapes[s].subscene = id;
            int di = displaced_of_shape[s];
            if (di >= 0)
                out.displaced_index[{(uint32_t)id, (uint32_t)out.shapes[s].geometry}] = di;
        }
        return id;
    }

    void add_instance(int subscene, const std::vector<int> &shape_indices, const Transform &to_world)
    {
        uint32_t inst = (uint32_t)out.scene.instances.size();
        out.scene.add_instance(device, subscene, to_world);
        for (int s : shape_indices) {
            out.shapes[s].instances.push_back(inst);
            out.shape_index[{inst, (uint32_t)out.shapes[s].geometry}] = s;
        }
    }

    void build()
    {
        displaced_of_shape.assign(spec.shapes.size(), -1);
        for (size_t di = 0; di < spec.displaced.size(); ++di)
            displaced_of_shape[spec.displaced[di].shape] = (int)di;
        out.shapes.resize(spec.shapes.size());
        out.displaced.assign(spec.displaced.size(), nullptr);
        out.substitutes.resize(spec.displaced.size());
        out.substitute_faces.resize(spec.displaced.size());

        for (const SceneObject &object : spec.objects) {
            if (object.mesh_asset) {
                int id = add_subscene(object.shapes);
                add_instance(id, object.shapes, object.to_world);
                continue;
            }
            // glTF: prototypes shared by the nodes that carry no
            // displacement, a subscene per node that does. Shapes are in
            // node order, prototype.meshes.size() per node.
            std::map<int, int> plain_subscene;
            size_t s = 0;
            for (const GltfAsset::Node &node : object.gltf->nodes) {
                size_t n_meshes = object.gltf->prototypes[node.mesh].meshes.size();
                std::vector<int> shapes(object.shapes.begin() + s, object.shapes.begin() + s + n_meshes);
                s += n_meshes;
                bool any_displaced = false;
                for (int sh : shapes)
                    any_displaced = any_displaced || displaced_of_shape[sh] >= 0;
                int id;
                if (any_displaced) {
                    id = add_subscene(shapes);
                } else {
                    auto it = plain_subscene.find(node.mesh);
                    if (it == plain_subscene.end())
                        it = plain_subscene.emplace(node.mesh, add_subscene(shapes)).first;
                    id = it->second;
                    for (int sh : shapes) {
                        out.shapes[sh].subscene = id;
                        out.shapes[sh].geometry = (int)(&sh - &shapes[0]);
                    }
                }
                add_instance(id, shapes, spec.shapes[shapes[0]].instances[0]);
            }
        }
        out.scene.create_rtc_scene(device);
        out.scene.build_area_lights();
    }
};

} // namespace

std::unique_ptr<DisplacedScene> create_displaced_scene(const SceneSpec &spec, const EmbreeDevice &device,
                                                       int substitute_subdivision)
{
    spec.require_valid();
    auto out = std::make_unique<DisplacedScene>();
    out->substitute_subdivision = substitute_subdivision;
    Builder builder{spec, device, *out, {}};
    builder.build();
    get_default_logger().info("create_displaced_scene: {} subscenes, {} instances, {} displaced objects{}",
                              out->scene.subscenes.size(), out->scene.instances.size(), spec.displaced.size(),
                              substitute_subdivision > 0
                                  ? " as texel-aligned substitutes at " + std::to_string(substitute_subdivision) +
                                        " sub-cells per texel"
                                  : "");
    return out;
}

} // namespace dmap
