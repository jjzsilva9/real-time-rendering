#version 430 core

layout(local_size_x = 256) in;

struct ChildDescriptor {
    uint topology;
    uint child_ptr;
    uint leaf_ptr;
    uint contour_ptr;
};

layout(std430, binding = 0) buffer NodePool {
    ChildDescriptor nodes[];
};

layout(std430, binding = 1) buffer LeafRadiancePool {
    vec4 leafRadiance[];
};

layout(std430, binding = 2) buffer FilteredRadiancePool {
    vec4 filteredRadiance[];
};

// Neighbor pool so we can reach the 27-voxel neighbourhood (3x3x3).
// Layout: n[0]=+X, n[1]=-X, n[2]=+Y, n[3]=-Y, n[4]=+Z, n[5]=-Z
struct Neighbors { uint n[6]; };
layout(std430, binding = 6) buffer NeighborPool {
    Neighbors neighborPointers[];
};

uniform uint levelStart;
uniform uint levelSize;

// Retrieve the filtered radiance of an arbitrary node index, or vec4(0) for missing/leaf nodes.
vec4 getFiltered(uint idx) {
    if (idx == 0u) return vec4(0.0);
    return filteredRadiance[idx];
}

// Return the pool index of the internal child at octant o inside node idx, or 0 if absent/leaf.
uint internalChild(uint nodeIdx, int o) {
    uint topo  = nodes[nodeIdx].topology;
    uint valid = (topo >> 24) & 0xFFu;
    uint leaf  = (topo >> 16) & 0xFFu;
    if (((valid >> o) & 1u) == 0u) return 0u;
    if (((leaf  >> o) & 1u) != 0u) return 0u;
    uint childPtr = nodes[nodeIdx].child_ptr;
    if (childPtr == 0u) return 0u;
    uint off = 0u;
    for (int j = 0; j < o; j++)
        if (((valid >> j) & 1u) != 0u && ((leaf >> j) & 1u) == 0u) off++;
    return childPtr + off;
}

// Return the pool index of the leaf at octant o inside node idx, or 0 if absent/internal.
uint leafIndex(uint nodeIdx, int o) {
    uint topo  = nodes[nodeIdx].topology;
    uint valid = (topo >> 24) & 0xFFu;
    uint leaf  = (topo >> 16) & 0xFFu;
    if (((valid >> o) & 1u) == 0u) return 0u;
    if (((leaf  >> o) & 1u) == 0u) return 0u;
    uint leafPtr = nodes[nodeIdx].leaf_ptr;
    uint off = 0u;
    for (int j = 0; j < o; j++)
        if (((valid >> j) & 1u) != 0u && ((leaf >> j) & 1u) != 0u) off++;
    return leafPtr + off;
}

// Sample the radiance (filtered or leaf) of octant o inside node idx.
vec4 sampleOctant(uint nodeIdx, int o) {
    uint topo  = nodes[nodeIdx].topology;
    uint valid = (topo >> 24) & 0xFFu;
    uint leaf  = (topo >> 16) & 0xFFu;
    if (((valid >> o) & 1u) == 0u) return vec4(0.0);
    if (((leaf  >> o) & 1u) != 0u) {
        uint li = leafIndex(nodeIdx, o);
        return li == 0u ? vec4(0.0) : leafRadiance[li];
    } else {
        uint ci = internalChild(nodeIdx, o);
        return getFiltered(ci);
    }
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= levelSize) return;

    uint nodeIdx = levelStart + idx;

    // -------------------------------------------------------------------
    // 3x3x3 Gaussian MIP-mapping (Crassin et al. 2011, Section 4.2.3)
    //
    // Each node occupies one "cell" of its level's grid. Its 8 children
    // (octants) occupy the 8 sub-cells of that cell. To compute a 3x3x3
    // Gaussian-weighted average centred on this node we need the 8 direct
    // children (weight 1/8 each = centre of the 3x3x3 kernel) plus
    // the children of the 6 face-adjacent neighbours (weight 1/16 each),
    // plus the children of the 12 edge neighbours (weight 1/32 each), plus
    // the children of the 8 corner neighbours (weight 1/64 each).
    //
    // This is exactly the separable 1-2-1 Gaussian kernel in 3D:
    //   weights in 1D: [1/4, 1/2, 1/4]
    //   product of three 1D kernels gives the 3D weights.
    //
    // Because the lower-level voxels are "evenly distributed" (shared
    // across adjacent node boundaries), each voxel appears in multiple
    // neighbouring bricks, hence the inverse-multiplicity Gaussian weights
    // shown in Figure 4 of the paper (1/1, 1/2, 1/4 at increasing distance).
    //
    // In our sparse structure we only have direct children.  We approximate
    // the Gaussian by gathering:
    //   - the 8 direct children of this node            (centre, w=1)
    //   - the facing children of the 6 axis neighbours  (face,   w=1/2)
    // and normalising.  This matches the paper's Pass 1 (centre from 27
    // values) and Pass 2 (face-adjacent half-weight), skipping Pass 3 (corner
    // octants) which contributes only 1/4 weight from far-away geometry.
    // -------------------------------------------------------------------

    vec4 sum  = vec4(0.0);
    float wSum = 0.0;

    // --- Centre: 8 direct children (octants 0..7) ---
    for (int o = 0; o < 8; o++) {
        sum  += sampleOctant(nodeIdx, o) * 1.0;
        wSum += 1.0;
    }

    // --- Faces: 6 axis-aligned neighbours ---
    // For each neighbour, we only gather the child octant that faces *back*
    // towards our node (the "mirror" octant), weighted 0.5.
    // Axis encoding: dir 0=+X(bit0), 1=-X, 2=+Y(bit1), 3=-Y, 4=+Z(bit2), 5=-Z
    // The mirror octant for each direction is the set of octants whose
    // corresponding bit is 0 (for +X it's the -X-side children: 0,2,4,6)
    Neighbors nb = neighborPointers[nodeIdx];

    // +X neighbour: its children on the -X side (bit0==0): octants 0,2,4,6
    if (nb.n[0] != 0u) {
        uint nIdx = nb.n[0];
        for (int o = 0; o < 8; o++) if ((o & 1) == 0) { sum += sampleOctant(nIdx, o) * 0.5; wSum += 0.5; }
    }
    // -X neighbour: children on the +X side (bit0==1): octants 1,3,5,7
    if (nb.n[1] != 0u) {
        uint nIdx = nb.n[1];
        for (int o = 0; o < 8; o++) if ((o & 1) == 1) { sum += sampleOctant(nIdx, o) * 0.5; wSum += 0.5; }
    }
    // +Y neighbour: children on the -Y side (bit1==0): octants 0,1,4,5
    if (nb.n[2] != 0u) {
        uint nIdx = nb.n[2];
        for (int o = 0; o < 8; o++) if ((o & 2) == 0) { sum += sampleOctant(nIdx, o) * 0.5; wSum += 0.5; }
    }
    // -Y neighbour: children on the +Y side (bit1==1): octants 2,3,6,7
    if (nb.n[3] != 0u) {
        uint nIdx = nb.n[3];
        for (int o = 0; o < 8; o++) if ((o & 2) == 2) { sum += sampleOctant(nIdx, o) * 0.5; wSum += 0.5; }
    }
    // +Z neighbour: children on the -Z side (bit2==0): octants 0,1,2,3
    if (nb.n[4] != 0u) {
        uint nIdx = nb.n[4];
        for (int o = 0; o < 8; o++) if ((o & 4) == 0) { sum += sampleOctant(nIdx, o) * 0.5; wSum += 0.5; }
    }
    // -Z neighbour: children on the +Z side (bit2==1): octants 4,5,6,7
    if (nb.n[5] != 0u) {
        uint nIdx = nb.n[5];
        for (int o = 0; o < 8; o++) if ((o & 4) == 4) { sum += sampleOctant(nIdx, o) * 0.5; wSum += 0.5; }
    }

    // Normalise by 8 (not by wSum) so alpha represents density correctly:
    // a fully-occupied node should have alpha = average child alpha / 8,
    // giving the premultiplied density expected by the cone accumulator.
    if (wSum > 0.0)
        filteredRadiance[nodeIdx] = sum / 8.0;
    else
        filteredRadiance[nodeIdx] = vec4(0.0);
}
